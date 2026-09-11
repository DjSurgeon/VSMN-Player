from conan import ConanFile
from conan.tools.cmake import cmake_layout

class IptvPlayerConan(ConanFile):
    name = "iptv_player"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("spdlog/1.13.0")
        self.requires("gtest/1.14.0")
        self.requires("libcurl/8.10.1")

    def build_requirements(self):
        self.test_requires("cpp-httplib/0.15.3")