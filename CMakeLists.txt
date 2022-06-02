cmake_minimum_required(VERSION 3.16)

project(
  dice-template-library
  VERSION 0.1.0
  DESCRIPTION
    "This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty handy. It contains `switch_cases` (Use runtime values in compile-time context) and `IntegralTemplatedTuple` (Create a tuple-like structure that instantiates a template for a range of values)."
  HOMEPAGE_URL "https://dice-research.org/")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED True)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_library(dice-template-library INTERFACE)
add_library(dice-template-library::dice-template-library ALIAS dice-template-library)

target_include_directories(
  dice-template-library
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(dice-template-library INTERFACE cxx_std_20)

include(cmake/header-only-install.cmake)
installheaderonly(dice-template-library include/Dice)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
