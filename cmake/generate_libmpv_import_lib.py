#!/usr/bin/env python3
"""Generate MSVC import library (libmpv-2.lib) from libmpv-2.dll exports."""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path


def read_cstring(data: bytes, offset: int) -> str:
    end = data.find(b"\x00", offset)
    if end < 0:
        raise ValueError("unterminated string")
    return data[offset:end].decode("ascii", errors="replace")


def parse_pe_exports(dll_path: Path) -> list[str]:
    data = dll_path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError(f"{dll_path} is not a PE file")

    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError(f"{dll_path} has invalid PE signature")

    machine, _sections, _, _, _, opt_header_size, _ = struct.unpack_from(
        "<HHIIIHH", data, pe_offset + 4
    )
    if machine not in (0x8664, 0xAA64):
        raise ValueError(f"unsupported machine type: 0x{machine:04x}")

    opt_offset = pe_offset + 24
    magic = struct.unpack_from("<H", data, opt_offset)[0]
    if magic != 0x20B:
        raise ValueError("expected PE32+ executable")

    export_rva, export_size = struct.unpack_from("<II", data, opt_offset + 112)
    if export_rva == 0 or export_size == 0:
        raise ValueError("no export directory")

    def rva_to_offset(rva: int) -> int:
        section_offset = opt_offset + opt_header_size
        for _ in range(struct.unpack_from("<H", data, pe_offset + 6)[0]):
            virt_size, virt_addr, raw_size, raw_ptr = struct.unpack_from(
                "<IIII", data, section_offset + 8
            )
            if virt_addr <= rva < virt_addr + max(virt_size, raw_size):
                return raw_ptr + (rva - virt_addr)
            section_offset += 40
        raise ValueError(f"could not map RVA 0x{rva:x}")

    export_offset = rva_to_offset(export_rva)
    (
        _characteristics,
        _time_date_stamp,
        _major,
        _minor,
        _name_rva,
        ordinal_base,
        num_functions,
        num_names,
        functions_rva,
        names_rva,
        ordinals_rva,
    ) = struct.unpack_from("<IIHHIIIIIII", data, export_offset)

    names_offset = rva_to_offset(names_rva)
    ordinals_offset = rva_to_offset(ordinals_rva)

    exports: list[str] = []
    for index in range(num_names):
        name_offset = rva_to_offset(
            struct.unpack_from("<I", data, names_offset + index * 4)[0]
        )
        exports.append(read_cstring(data, name_offset))

    if not exports:
        raise ValueError("no exported symbols found")

    return sorted(set(exports))


def find_lib_exe() -> Path | None:
    program_files_x86 = Path(
        environ_get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    )
    vswhere = program_files_x86 / "Microsoft Visual Studio/Installer/vswhere.exe"
    if vswhere.is_file():
        for pattern in (
            r"VC\Tools\MSVC\*\bin\Hostx64\x64\lib.exe",
            r"VC\Tools\MSVC\*\bin\Hostarm64\arm64\lib.exe",
        ):
            result = subprocess.run(
                [
                    str(vswhere),
                    "-latest",
                    "-prerelease",
                    "-requires",
                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                    "-find",
                    pattern,
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            for line in result.stdout.splitlines():
                candidate = Path(line.strip())
                if candidate.is_file():
                    return candidate

    for root in (
        Path(r"C:\Program Files\Microsoft Visual Studio"),
        Path(r"C:\Program Files (x86)\Microsoft Visual Studio"),
    ):
        if not root.is_dir():
            continue
        matches = sorted(root.glob("**/Hostx64/x64/lib.exe"))
        if matches:
            return matches[-1]
    return None


def environ_get(name: str, default: str) -> str:
    import os

    return os.environ.get(name, default)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    dll_path = args.dll.resolve()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    exports = parse_pe_exports(dll_path)
    def_path = out_dir / "libmpv-2.def"
    lib_path = out_dir / "libmpv-2.lib"

    def_lines = ["LIBRARY libmpv-2", "EXPORTS", *exports]
    def_path.write_text("\n".join(def_lines) + "\n", encoding="ascii")

    lib_exe = find_lib_exe()
    if lib_exe is None:
        print(
            "Wrote",
            def_path,
            "but lib.exe was not found. Install Visual Studio Build Tools, then run:\n"
            f'  "{def_path}" -> lib /def:"{def_path}" /name:libmpv-2.dll /out:"{lib_path}" /machine:x64',
            file=sys.stderr,
        )
        return 1

    machine = "x64" if "arm64" not in str(lib_exe).lower() else "arm64"
    subprocess.run(
        [
            str(lib_exe),
            f"/def:{def_path}",
            "/name:libmpv-2.dll",
            f"/out:{lib_path}",
            f"/machine:{machine}",
        ],
        check=True,
    )

    print(f"Generated {lib_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
