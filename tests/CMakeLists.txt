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
  target_link_libraries(${ARGV0} PRIVATE doctest DiceTemplateLibrary)
  add_dependencies(build_tests ${ARGV0})
endmacro()

add_executable(tests_SwitchTemplateFunctions tests_SwitchTemplateFunctions.cpp)
custom_add_test(tests_SwitchTemplateFunctions)

add_executable(tests_IntegralTemplatedTuple tests_IntegralTemplatedTuple.cpp)
custom_add_test(tests_IntegralTemplatedTuple)
