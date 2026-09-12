from conan import ConanFile
from conan.tools.cmake import cmake_layout

class IptvPlayerConan(ConanFile):
    name = "iptv_player"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    
    # We want ImGui to come with its official bindings for our backend
    # This prevents us from having to copy imgui_impl_sdl2.cpp manually
    default_options = {
        "imgui/*:shared": False,
    }

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        # Network & Core
        self.requires("spdlog/1.13.0")
        self.requires("libcurl/8.10.1")
        
        # Testing
        self.requires("gtest/1.14.0")
        
        # Audio / Video Decoding (Heavy Artillery)
        self.requires("ffmpeg/6.1.1")
        
        # Rendering & Windowing
        self.requires("sdl/2.28.5")
        self.requires("opengl/system")
        self.requires("imgui/1.90.4")

    def build_requirements(self):
        self.test_requires("cpp-httplib/0.15.3")
        self.test_requires("benchmark/1.8.3")