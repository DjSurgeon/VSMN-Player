from conan import ConanFile
from conan.tools.cmake import cmake_layout


class IptvPlayerConan(ConanFile):
    name = "iptv_player"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    # Lean media stack: avcodec, avformat, avutil and swresample only.
    # Everything else is off to drop the ALSA/PulseAudio/X11 subtree, whose
    # system requirements cannot be met in a non-root container.
    default_options = {
        "ffmpeg/6.1:avdevice": False,
        "ffmpeg/6.1:avfilter": False,
        "ffmpeg/6.1:postproc": False,
        "ffmpeg/6.1:swscale": False,
        "ffmpeg/6.1:with_bzip2": False,
        "ffmpeg/6.1:with_fontconfig": False,
        "ffmpeg/6.1:with_freetype": False,
        "ffmpeg/6.1:with_fribidi": False,
        "ffmpeg/6.1:with_harfbuzz": False,
        "ffmpeg/6.1:with_libalsa": False,
        "ffmpeg/6.1:with_libaom": False,
        "ffmpeg/6.1:with_libdav1d": False,
        "ffmpeg/6.1:with_libdrm": False,
        "ffmpeg/6.1:with_libfdk_aac": False,
        "ffmpeg/6.1:with_libiconv": False,
        "ffmpeg/6.1:with_libmp3lame": False,
        "ffmpeg/6.1:with_libsvtav1": False,
        "ffmpeg/6.1:with_libvpx": False,
        "ffmpeg/6.1:with_libwebp": False,
        "ffmpeg/6.1:with_libx264": False,
        "ffmpeg/6.1:with_libx265": False,
        "ffmpeg/6.1:with_libxml2": False,
        "ffmpeg/6.1:with_lzma": False,
        "ffmpeg/6.1:with_openh264": False,
        "ffmpeg/6.1:with_openjpeg": False,
        "ffmpeg/6.1:with_opus": False,
        "ffmpeg/6.1:with_programs": False,
        "ffmpeg/6.1:with_pulse": False,
        "ffmpeg/6.1:with_sdl": False,
        "ffmpeg/6.1:with_soxr": False,
        "ffmpeg/6.1:with_vaapi": False,
        "ffmpeg/6.1:with_vdpau": False,
        "ffmpeg/6.1:with_vorbis": False,
        "ffmpeg/6.1:with_vulkan": False,
        "ffmpeg/6.1:with_xcb": False,
        "ffmpeg/6.1:with_xlib": False,
        "ffmpeg/6.1:with_zeromq": False,
    }

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("libcurl/8.10.1")
        self.requires("ffmpeg/6.1")
        self.requires("gtest/1.14.0")

    def build_requirements(self):
        self.test_requires("cpp-httplib/0.15.3")
        self.test_requires("benchmark/1.8.3")
