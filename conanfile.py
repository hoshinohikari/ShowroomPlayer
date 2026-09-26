from conan import ConanFile
from conan.tools.cmake import CMakeDeps


class ShowroomPlayerDependencies(ConanFile):
    name = "showroom-player-dependencies"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("spdlog/[>=1.17.0]")

    def configure(self):
        self.options["spdlog/*"].header_only = True
        self.options["fmt/*"].header_only = True
        if str(self.settings.os) == "Windows":
            self.options["spdlog/*"].wchar_filenames = True

    def generate(self):
        CMakeDeps(self).generate()
