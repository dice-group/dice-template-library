set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS)
find_package(DocTest REQUIRED)

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
    ${ARGV0} PRIVATE doctest::doctest dice-template-library::dice-template-library)
  add_dependencies(build_tests ${ARGV0})
endmacro()

add_executable(tests_switch_template_functions tests_switch_cases.cpp)
custom_add_test(tests_switch_template_functions)

add_executable(tests_integral_templated_tuple
        tests_integral_template_tuple.cpp)
custom_add_test(tests_integral_templated_tuple)

add_executable(tests_integral_templated_tuple_v2
        tests_integral_template_tuple_v2.cpp)
custom_add_test(tests_integral_templated_tuple_v2)

add_executable(tests_integral_template_variant
        tests_integral_template_variant.cpp)
custom_add_test(tests_integral_template_variant)

add_executable(tests_integral_template_variant_v2
        tests_integral_template_variant_v2.cpp)
custom_add_test(tests_integral_template_variant_v2)

add_executable(tests_for tests_for.cpp)
custom_add_test(tests_for)

add_executable(tests_polymorphic_allocator tests_polymorphic_allocator.cpp)
target_link_libraries(tests_polymorphic_allocator PRIVATE Boost::headers)

custom_add_test(tests_polymorphic_allocator)

add_executable(tests_overloaded tests_overloaded.cpp)
custom_add_test(tests_overloaded)

add_executable(tests_defer tests_defer.cpp)
custom_add_test(tests_defer)

add_executable(tests_tuple_algorithm tests_tuple_algorithm.cpp)
custom_add_test(tests_tuple_algorithm)

add_executable(tests_flex_array tests_flex_array.cpp)
custom_add_test(tests_flex_array)

add_executable(tests_fmt_join tests_fmt_join.cpp)
custom_add_test(tests_fmt_join)

add_executable(tests_generator tests_generator.cpp)
custom_add_test(tests_generator)

add_executable(tests_channel tests_channel.cpp)
custom_add_test(tests_channel)

add_executable(tests_variant2 tests_variant2.cpp)
custom_add_test(tests_variant2)

add_executable(tests_type_traits tests_type_traits.cpp)
custom_add_test(tests_type_traits)

add_executable(tests_limit_allocator tests_limit_allocator.cpp)
custom_add_test(tests_limit_allocator)

add_executable(tests_memfn tests_memfn.cpp)
custom_add_test(tests_memfn)

add_executable(tests_pool_allocator tests_pool_allocator.cpp)
custom_add_test(tests_pool_allocator)

add_executable(tests_ranges tests_ranges.cpp)
custom_add_test(tests_ranges)

add_executable(tests_mutex tests_mutex.cpp)
custom_add_test(tests_mutex)

add_executable(tests_shared_mutex tests_shared_mutex.cpp)
custom_add_test(tests_shared_mutex)

add_executable(tests_static_string tests_static_string.cpp)
custom_add_test(tests_static_string)

add_executable(tests_next_to_range tests_next_to_range.cpp)
custom_add_test(tests_next_to_range)

add_executable(tests_inplace_polymorphic tests_inplace_polymorphic.cpp)
custom_add_test(tests_inplace_polymorphic)

add_executable(tests_type_list tests_type_list.cpp)
custom_add_test(tests_type_list)

add_executable(tests_lazy_conditional tests_lazy_conditional.cpp)
custom_add_test(tests_lazy_conditional)

add_executable(tests_standard_layout_tuple tests_standard_layout_tuple.cpp)
custom_add_test(tests_standard_layout_tuple)

add_executable(tests_format_to_ostream tests_format_to_ostream.cpp)
custom_add_test(tests_format_to_ostream)

add_executable(tests_stdint tests_stdint.cpp)
custom_add_test(tests_stdint)

add_executable(tests_functional tests_functional.cpp)
custom_add_test(tests_functional)

add_executable(tests_macro_util tests_macro_util.cpp)
custom_add_test(tests_macro_util)
