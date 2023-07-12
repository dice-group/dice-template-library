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
    topics = ("template", "template-library", "compile-time", "switch", "integral-tuple")
    settings = "build_type", "compiler", "os", "arch"
    generators = ("CMakeDeps", "CMakeToolchain")
    exports_sources = "include/*", "CMakeLists.txt", "cmake/*", "LICENSE"
    no_copy_source = True
    options = {"with_boost": [True, False]}
    default_options = {"with_boost": False}

    def requirements(self):
        if self.options.with_boost:
            self.requires("boost/1.81.0")

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

    def layout(self):
        cmake_layout(self)

    def package_id(self):
        self.info.header_only()

    def package(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.install()

        for dir in ("lib", "res", "share"):
            rmdir(self, os.path.join(self.package_folder, dir))

        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
