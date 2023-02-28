import os

from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake
from conan.tools.layout import cmake_layout
from conans import ConanFile

required_conan_version = ">=1.59.0"


class TestPackageConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)

    def _min_cppstd(self):
        return "20"

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, self._min_cppstd)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        self.run(os.path.join(self.cpp.build.bindirs[0], "test_package"), run_environment=True)
