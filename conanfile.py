from conan import ConanFile
from conan.tools.cmake import CMakeConfigDeps, CMakeToolchain, cmake_layout


class CclawRecipe(ConanFile):
    name = "cclaw"
    settings = "os", "compiler", "build_type", "arch"

    def configure(self):
        os_ = str(self.settings.os)

        if os_ == "Windows":
            self.options["cpr/*"].with_ssl = "winssl"
            self.options["libcurl/*"].with_ssl = "schannel"

        self.options["stduuid/*"].with_cxx20_span = False
        self.options["spdlog/*"].header_only = True
        self.options["fmt/*"].header_only = True

        self.options["grpc/*"].csharp_plugin = False
        self.options["grpc/*"].node_plugin = False
        self.options["grpc/*"].objective_c_plugin = False
        self.options["grpc/*"].php_plugin = False
        self.options["grpc/*"].python_plugin = False
        self.options["grpc/*"].ruby_plugin = False

        self.options["boost/*"].system_no_deprecated = True
        self.options["boost/*"].asio_no_deprecated = True
        self.options["boost/*"].filesystem_no_deprecated = True
        self.options["boost/*"].filesystem_use_std_fs = True
        self.options["boost/*"].without_cobalt = True
        pass

    def requirements(self):
        # self.requires("cpr/[>=1]")
        pass

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeConfigDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, generator="Ninja Multi-Config")
        tc.presets_prefix = str(self.settings.os)
        tc.generate()
