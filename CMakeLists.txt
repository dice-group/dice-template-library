cmake_minimum_required(VERSION 3.16)

project(
  dice-template-library
  VERSION 1.0.0
  DESCRIPTION
    "This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty handy. It contains: `switch_cases` (Use runtime values in compile-time context), `integral_template_tuple` (Create a tuple-like structure that instantiates a template for a range of values), `integral_template_variant` (A wrapper type for `std::variant` guarantees to only contain variants of the form `T<IX>` and `for_{types,values,range}` (Compile time for loops for types, values or ranges))."
  HOMEPAGE_URL "https://dice-research.org/")

option(USE_CONAN "If available, use conan to retrieve dependencies." OFF)
option(WITH_BOOST "Enables dice::template_library::offset_ptr_stl_allocator" OFF)

if (PROJECT_IS_TOP_LEVEL AND USE_CONAN AND WITH_BOOST)
  include(cmake/conan_cmake.cmake)
  set(CONAN_OPTIONS "with_boost=True")
  install_packages_via_conan("${CMAKE_SOURCE_DIR}/conanfile.py" "${CONAN_OPTIONS}")
endif ()

if (WITH_BOOST)
  find_package(Boost REQUIRED COMPONENTS)
  set(DICE_TEMPLATE_LIB_BOOST_DEP "Boost::headers")
endif ()

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        )

target_link_libraries(${PROJECT_NAME} INTERFACE
        ${DICE_TEMPLATE_LIB_BOOST_DEP}
        )

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        )

if (WITH_BOOST)
  target_compile_definitions(${PROJECT_NAME} INTERFACE DICE_TEMPLATE_LIBRARY_WITH_BOOST=1)
endif ()

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
