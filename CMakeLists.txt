cmake_minimum_required(VERSION 3.16)

project(DiceTemplateLibrary
  VERSION 0.1
  DESCRIPTION "A small collection of usefull template constructs"
  HOMEPAGE_URL "https://dice-research.org/"
)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED True)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

add_library(DiceTemplateLibrary INTERFACE)
add_library(DiceTemplateLibrary::DiceTemplateLibrary ALIAS DiceTemplateLibrary)

target_include_directories (
  DiceTemplateLibrary INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(DiceTemplateLibrary INTERFACE cxx_std_20)

include(cmake/headerOnlyInstall.cmake)
installHeaderOnly(DiceTemplateLibrary include/Dice)


if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
