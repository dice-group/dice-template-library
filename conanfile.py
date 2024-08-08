import os
import re

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import load, rmdir, copy


class DiceTemplateLibrary(ConanFile):
    author = "DICE Group <info@dice-research.org>"
    description = None
    homepage = "https://dice-research.org/"
    url = "https://github.com/dice-group/dice-template-library.git"
    license = "MIT"
    topics = "template", "template-library", "compile-time", "switch", "integral-tuple"
    package_type = "header-library"
    generators = "CMakeDeps", "CMakeToolchain"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "include/*", "CMakeLists.txt", "cmake/*", "LICENSE"
    no_copy_source = True
    options = {
        "with_test_deps": [True, False],
        "with_svector": [True, False],
        "with_boost": [True, False],
    }
    default_options = {
        "with_test_deps": False,
        "with_svector": False,
        "with_boost": False,
    }

    def requirements(self):
        if self.options.with_svector:
            self.requires("svector/1.0.3", transitive_headers=True)

        if self.options.with_boost:
            self.requires("boost/1.84.0", transitive_headers=True)

        if self.options.with_test_deps:
            self.test_requires("doctest/2.4.11")

    def layout(self):
        cmake_layout(self)

    def build(self):
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake = CMake(self)
            cmake.configure(variables={"WITH_SVECTOR": self.options.with_svector, "WITH_BOOST": self.options.with_boost})
            cmake.build()

    def package_id(self):
        self.info.clear()

    def set_name(self):
        if not hasattr(self, 'name') or self.version is None:
            cmake_file = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.name = re.search(r"project\(\s*([a-z\-]+)\s+VERSION", cmake_file).group(1)

    def set_version(self):
        if not hasattr(self, 'version') or self.version is None:
            cmake_file = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.version = re.search(r"project\([^)]*VERSION\s+(\d+\.\d+.\d+)[^)]*\)", cmake_file).group(1)
        if not hasattr(self, 'description') or self.description is None:
            cmake_file = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.description = re.search(r"project\([^)]*DESCRIPTION\s+\"([^\"]+)\"[^)]*\)", cmake_file).group(1)

    def package(self):
        cmake = CMake(self)
        cmake.install()

        for dir in ("lib", "res", "share"):
            rmdir(self, os.path.join(self.package_folder, dir))

        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.requires = []

        if self.options.with_svector:
            self.cpp_info.requires += ["svector::svector"]

        if self.options.with_boost:
            self.cpp_info.requires += ["boost::headers"]

        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_target_name", f"{self.name}::{self.name}")
        self.cpp_info.set_property("cmake_file_name", self.name)
