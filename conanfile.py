import os
import re
from conans import ConanFile, CMake
from conans.tools import load
from conans.util.files import rmdir


class DiceTemplateLibrary(ConanFile):
    author = "DICE Group <info@dice-research.org>"
    description = None
    homepage = "https://dice-research.org/"
    url = "https://github.com/dice-group/dice-template-library.git"
    license = "MIT"
    topics = ("template", "template library", "compile-time", "switch", "integral tuple")
    settings = "build_type", "compiler", "os", "arch"
    generators = ("CMakeDeps", "CMakeToolchain")
    exports = "LICENSE"
    exports_sources = "include/*", "CMakeLists.txt", "cmake/*"
    no_copy_source = True

    def set_name(self):
        if not hasattr(self, 'name') or self.version is None:
            cmake_file = load(os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.name = re.search(r"project\(\s*([a-z\-]+)\s+VERSION", cmake_file).group(1)

    def set_version(self):
        if not hasattr(self, 'version') or self.version is None:
            cmake_file = load(os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.version = re.search(r"project\([^)]*VERSION\s+(\d+\.\d+.\d+)[^)]*\)", cmake_file).group(1)
        if not hasattr(self, 'description') or self.description is None:
            cmake_file = load(os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.description = re.search(r"project\([^)]*DESCRIPTION\s+\"([^\"]+)\"[^)]*\)", cmake_file).group(1)

    def package(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.install()
        rmdir(os.path.join(self.package_folder, "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "dice::template-library")

