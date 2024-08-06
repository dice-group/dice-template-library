cmake_minimum_required(VERSION 3.22)
project(test_package)

find_package(dice-template-library REQUIRED)

add_executable(${PROJECT_NAME} test_package.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
        dice-template-library::dice-template-library)

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        )
