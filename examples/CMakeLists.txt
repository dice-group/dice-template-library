set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(examples_for examples_for.cpp)
target_link_libraries(examples_for
                      PRIVATE dice-template-library::dice-template-library)

add_executable(examples_switch_template_functions examples_switch_cases.cpp)
target_link_libraries(examples_switch_template_functions
                      PRIVATE dice-template-library::dice-template-library)

add_executable(examples_integral_templated_tuple
               examples_integral_template_tuple.cpp)
target_link_libraries(examples_integral_templated_tuple
                      PRIVATE dice-template-library::dice-template-library)

add_executable(examples_integral_template_variant
        examples_integral_template_variant.cpp)
target_link_libraries(examples_integral_template_variant
        PRIVATE dice-template-library::dice-template-library)


find_package(Boost REQUIRED COMPONENTS)

add_executable(examples_polymorphic_allocator
        examples_polymorphic_allocator.cpp)
target_link_libraries(examples_polymorphic_allocator
        PRIVATE
        dice-template-library::dice-template-library
        Boost::headers
        )
