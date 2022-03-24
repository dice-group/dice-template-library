import os
import re
from conans import ConanFile, CMake
from conans.tools import load


class DiceTemplateLibrary(ConanFile):
    name = "DiceTemplateLibrary"
    author = "DICE Group <info@dice-research.org>"
    description = None
    homepage = "https://dice-research.org/"
    url = homepage
    settings = "build_type", "compiler", "os", "arch"
    generators = "cmake", "cmake_find_package", "cmake_paths"
    exports_sources = "include/*", "CMakeLists.txt", "cmake/*"
    no_copy_source = True

    def set_version(self):
        if not hasattr(self, 'version') or self.version is None:
            cmake_file = load(os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.version = re.search(r"project\([^)]*VERSION\s+(\d+\.\d+)[^)]*\)", cmake_file).group(1)
        if not hasattr(self, 'description') or self.description is None:
            cmake_file = load(os.path.join(self.recipe_folder, "CMakeLists.txt"))
            self.description = re.search(r"project\([^)]*DESCRIPTION\s+\"([^\"]+)\"[^)]*\)", cmake_file).group(1)

    def package(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.install()

    def package_id(self):
        self.info.header_only()
