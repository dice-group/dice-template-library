cmake_minimum_required(VERSION 3.28)

project(
  dice-template-library
  VERSION 2.7.1
  DESCRIPTION
    "This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty handy. It contains: `switch_cases` (Use runtime values in compile-time context), `integral_template_tuple` (Create a tuple-like structure that instantiates a template for a range of values), `integral_template_variant` (A wrapper type for `std::variant` guarantees to only contain variants of the form `T<IX>` and `for_{types,values,range}` (Compile time for loops for types, values or ranges))."
  HOMEPAGE_URL "https://dice-research.org/")
set(POBR_VERSION 2)  # Persisted Object Binary Representation

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cppm.in ${CMAKE_CURRENT_SOURCE_DIR}/src/dice/template-library/version.cppm)

option(BUILD_TESTING "build tests" OFF)
option(BUILD_EXAMPLES "build examples" OFF)
option(WITH_SVECTOR "use ankerl/svector to provide flex_array with flex_array_implementation::sbo_vector" OFF)
option(WITH_BOOST "use boost to provide offset_ptr_stl_allocator in polymorphic_allocator.hpp" OFF)

if (PROJECT_IS_TOP_LEVEL)
    set(style_files
        .clang-format
        .clang-tidy
    )

    foreach (style_file ${style_files})
        file(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/${style_file})
        file(DOWNLOAD https://raw.githubusercontent.com/dice-group/tentris-cpp-coding-guidelines/refs/heads/main/.clang-format ${CMAKE_CURRENT_SOURCE_DIR}/${style_file})
    endforeach ()

    if (BUILD_TESTING OR BUILD_EXAMPLES)
        set(WITH_BOOST ON)
        set(CONAN_INSTALL_ARGS "${CONAN_INSTALL_ARGS};-o=&:with_test_deps=True")
    endif ()

    if (WITH_SVECTOR)
        set(CONAN_INSTALL_ARGS "${CONAN_INSTALL_ARGS};-o=&:with_svector=True")
    endif ()

    if (WITH_BOOST)
        set(CONAN_INSTALL_ARGS "${CONAN_INSTALL_ARGS};-o=&:with_boost=True;-o=boost/*:header_only=True")
    endif ()
endif ()

add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC FILE_SET CXX_MODULES BASE_DIRS $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> FILES
        src/dice/template-library/dtl.cppm
        src/dice/template-library/version.cppm
        src/dice/template-library/channel.cppm
        src/dice/template-library/flex_array.cppm
        src/dice/template-library/fmt_join.cppm
        src/dice/template-library/for.cppm
        src/dice/template-library/format_to_ostream.cppm
        src/dice/template-library/functional.cppm
        src/dice/template-library/inplace_polymorphic.cppm
        src/dice/template-library/integral_template_tuple.cppm
        src/dice/template-library/integral_sequence.cppm
        src/dice/template-library/integral_template_variant.cppm
        src/dice/template-library/lazy_conditional.cppm
        src/dice/template-library/limit_allocator.cppm
        src/dice/template-library/mutex.cppm
        src/dice/template-library/next_to_range.cppm
        src/dice/template-library/opt_minmax.cppm
        src/dice/template-library/ranges.cppm
        src/dice/template-library/overloaded.cppm
        src/dice/template-library/pointer_tag_pair.cppm
        src/dice/template-library/polymorphic_allocator.cppm
        src/dice/template-library/pool_allocator.cppm
        src/dice/template-library/shared_mutex.cppm
        src/dice/template-library/static_string.cppm
        src/dice/template-library/stdint.cppm
        src/dice/template-library/switch_cases.cppm

        src/dice/template-library/standard_layout_tuple.cppm
        src/dice/template-library/tuple_algorithm.cppm
        src/dice/template-library/type_traits.cppm
        src/dice/template-library/type_list.cppm
        src/dice/template-library/variant2.cppm
)

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        )

if (WITH_SVECTOR)
    find_package(svector REQUIRED)

    target_link_libraries(${PROJECT_NAME} PUBLIC
            svector::svector
            )
endif ()

if (WITH_BOOST)
    find_package(Boost REQUIRED COMPONENTS)

    target_link_libraries(${PROJECT_NAME} PUBLIC
            Boost::headers
            )
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        )
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

include(cmake/install_module_library.cmake)
install_module_library(${PROJECT_NAME} ${PROJECT_NAME} ${PROJECT_NAME} "include")

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
