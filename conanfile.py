from conan import ConanFile
from conan.tools.cmake import cmake_layout

class IptvPlayerConan(ConanFile):
    name = "iptv_player"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    
    # We want ImGui to come with its official bindings for our backend
    # This prevents us from having to copy imgui_impl_sdl2.cpp manually
    #
    # FFmpeg is pinned to a lean media stack: we only consume avcodec,
    # avformat, avutil and swresample. Everything else (device backends,
    # filters, scaler, CLI tools and third-party codec libraries) is turned
    # off because the project has no rendering or capture surface. This also
    # drops the ALSA/PulseAudio/X11 dependency subtree, which drags in
    # xorg/system system requirements that cannot be satisfied in a
    # non-root container.
    default_options = {
        "ffmpeg/6.1:with_libdrm": False,
        "ffmpeg/6.1:with_vaapi": False,
        "ffmpeg/6.1:with_vdpau": False,
        "ffmpeg/6.1:with_vulkan": False,
        "ffmpeg/6.1:with_soxr": False,
        "ffmpeg/6.1:with_zeromq": False,
        "ffmpeg/6.1:with_sdl": False,
        "ffmpeg/6.1:with_libxml2": False,
        "ffmpeg/6.1:avdevice": False,
        "ffmpeg/6.1:avfilter": False,
        "ffmpeg/6.1:postproc": False,
        "ffmpeg/6.1:swscale": False,
        "ffmpeg/6.1:with_programs": False,
        "ffmpeg/6.1:with_libalsa": False,
        "ffmpeg/6.1:with_pulse": False,
        "ffmpeg/6.1:with_xcb": False,
        "ffmpeg/6.1:with_xlib": False,
        "ffmpeg/6.1:with_freetype": False,
        "ffmpeg/6.1:with_fontconfig": False,
        "ffmpeg/6.1:with_fribidi": False,
        "ffmpeg/6.1:with_harfbuzz": False,
        "ffmpeg/6.1:with_libiconv": False,
        "ffmpeg/6.1:with_bzip2": False,
        "ffmpeg/6.1:with_lzma": False,
        "ffmpeg/6.1:with_openjpeg": False,
        "ffmpeg/6.1:with_openh264": False,
        "ffmpeg/6.1:with_vorbis": False,
        "ffmpeg/6.1:with_opus": False,
        "ffmpeg/6.1:with_libx264": False,
        "ffmpeg/6.1:with_libx265": False,
        "ffmpeg/6.1:with_libvpx": False,
        "ffmpeg/6.1:with_libmp3lame": False,
        "ffmpeg/6.1:with_libfdk_aac": False,
        "ffmpeg/6.1:with_libwebp": False,
        "ffmpeg/6.1:with_libsvtav1": False,
        "ffmpeg/6.1:with_libaom": False,
        "ffmpeg/6.1:with_libdav1d": False,
    }

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        # Network & Core
        self.requires("libcurl/8.10.1")

        # Media decoding
        self.requires("ffmpeg/6.1")

        # Testing
        self.requires("gtest/1.14.0")

    def build_requirements(self):
        self.test_requires("cpp-httplib/0.15.3")
        self.test_requires("benchmark/1.8.3")