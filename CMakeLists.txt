cmake_minimum_required(VERSION 3.24)

set(PACKAGE_MANAGER "")
if (CMAKE_PROJECT_TOP_LEVEL_INCLUDES MATCHES "conan_provider.cmake$")
    message("detected conan")
    set(PACKAGE_MANAGER "conan")
else ()
    message("conan not detected, defaulting to vcpkg")
    set(PACKAGE_MANAGER "vcpkg")
endif ()

option(BUILD_TESTING "build tests" OFF)
option(BUILD_EXAMPLES "build examples" OFF)
option(WITH_SVECTOR "use ankerl/svector to provide flex_array with flex_array_implementation::sbo_vector" OFF)
option(WITH_BOOST "use boost to provide offset_ptr_stl_allocator in polymorphic_allocator.hpp" OFF)

if (BUILD_TESTING OR BUILD_EXAMPLES)
    set(CONAN_INSTALL_ARGS "${CONAN_INSTALL_ARGS};-o=&:with_test_deps=True")
    list(APPEND VCPKG_MANIFEST_FEATURES "test")
endif ()

if (WITH_SVECTOR)
    set(CONAN_INSTALL_ARGS "${CONAN_INSTALL_ARGS};-o=&:with_svector=True")
    list(APPEND VCPKG_MANIFEST_FEATURES "svector")
endif ()

if (WITH_BOOST)
    set(CONAN_INSTALL_ARGS "${CONAN_INSTALL_ARGS};-o=&:with_boost=True;-o=boost/*:header_only=True")
    list(APPEND VCPKG_MANIFEST_FEATURES "boost")
endif ()

project(
  dice-template-library
  VERSION 1.14.0
  DESCRIPTION
    "This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty handy. It contains: `switch_cases` (Use runtime values in compile-time context), `integral_template_tuple` (Create a tuple-like structure that instantiates a template for a range of values), `integral_template_variant` (A wrapper type for `std::variant` guarantees to only contain variants of the form `T<IX>` and `for_{types,values,range}` (Compile time for loops for types, values or ranges))."
  HOMEPAGE_URL "https://dice-research.org/")
set(POBR_VERSION 1)  # Persisted Object Binary Representation

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/dice/template-library/version.hpp)

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

set(INSTALL_INTERFACE_LIB_DEPENDENCIES "")
if (WITH_SVECTOR)
    find_package(svector REQUIRED CONFIG)

    target_link_libraries(${PROJECT_NAME} INTERFACE
            svector::svector
            )
    string(APPEND INSTALL_INTERFACE_LIB_DEPENDENCIES "\nfind_package(svector REQUIRED CONFIG)")
endif ()

if (WITH_BOOST)
    if (PACKAGE_MANAGER STREQUAL "vcpkg")
        find_package(Boost REQUIRED CONFIG COMPONENTS interprocess pool)

        target_link_libraries(${PROJECT_NAME} INTERFACE
                Boost::interprocess
                Boost::pool
        )
        string(APPEND INSTALL_INTERFACE_LIB_DEPENDENCIES "\nfind_package(Boost REQUIRED CONFIG COMPONENTS interprocess pool)")
    else ()
        find_package(Boost REQUIRED COMPONENTS)
        target_link_libraries(${PROJECT_NAME} INTERFACE
                Boost::headers
        )
    endif ()
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        )

include(cmake/install_interface_library.cmake)
install_interface_library(${PROJECT_NAME} ${PROJECT_NAME} ${PROJECT_NAME} "include")

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
