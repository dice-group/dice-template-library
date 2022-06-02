add_executable(examples_switch_template_functions
               examples_switch_template_functions.cpp)
target_link_libraries(examples_switch_template_functions
                      PRIVATE dice-template-library)

add_executable(examples_integral_templated_tuple
               examples_integral_templated_tuple.cpp)
target_link_libraries(examples_integral_templated_tuple
                      PRIVATE dice-template-library)
