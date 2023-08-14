set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  DocTest
  GIT_REPOSITORY "https://github.com/doctest/doctest.git"
  GIT_TAG "v2.4.8"
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(DocTest)

add_custom_target(build_tests)
add_custom_target(run_tests COMMAND ${CMAKE_CTEST_COMMAND})
add_dependencies(run_tests build_tests)

macro(CUSTOM_ADD_TEST)
  if(NOT ${ARGC} EQUAL 1)
    message(
      FATAL_ERROR
        "Call to 'CUSTOM_ADD_TEST' with ${ARGC} arguments instead of 1")
  endif()
  add_test(NAME ${ARGV0} COMMAND ${ARGV0})
  target_link_libraries(
    ${ARGV0} PRIVATE doctest dice-template-library::dice-template-library)
  add_dependencies(build_tests ${ARGV0})
endmacro()

add_executable(tests_switch_template_functions tests_switch_cases.cpp)
custom_add_test(tests_switch_template_functions)

add_executable(tests_integral_templated_tuple
        tests_integral_template_tuple.cpp)
custom_add_test(tests_integral_templated_tuple)

add_executable(tests_integral_template_variant
        tests_integral_template_variant.cpp)
custom_add_test(tests_integral_template_variant)

add_executable(tests_for tests_for.cpp)
custom_add_test(tests_for)

add_executable(tests_polymorphic_allocator tests_polymorphic_allocator.cpp)
custom_add_test(tests_polymorphic_allocator)

if (WITH_BOOST)
  add_executable(tests_offset_ptr_stl_allocator tests_offset_ptr_stl_allocator.cpp)
  custom_add_test(tests_offset_ptr_stl_allocator)
endif ()
