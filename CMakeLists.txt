cmake_minimum_required(VERSION 3.16)

project(DiceTemplateLibrary)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif ()
