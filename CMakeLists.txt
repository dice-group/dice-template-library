cmake_minimum_required(VERSION 3.24)

project(
  dice-template-library
  VERSION 1.5.1
  DESCRIPTION
    "This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty handy. It contains: `switch_cases` (Use runtime values in compile-time context), `integral_template_tuple` (Create a tuple-like structure that instantiates a template for a range of values), `integral_template_variant` (A wrapper type for `std::variant` guarantees to only contain variants of the form `T<IX>` and `for_{types,values,range}` (Compile time for loops for types, values or ranges))."
  HOMEPAGE_URL "https://dice-research.org/")

option(BUILD_TESTING "build tests" OFF)
option(BUILD_EXAMPLES "build examples" OFF)

# conan requires cmake build type to be specified and it is generally a good idea
if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif ()
endif ()

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        )

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        )

include(cmake/install_interface_library.cmake)
install_interface_library(${PROJECT_NAME} ${PROJECT_NAME} ${PROJECT_NAME} "include")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
