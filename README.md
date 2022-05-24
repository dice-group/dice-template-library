# Dice Template Library

This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty handy.
It contains:
- `switch_cases`: Use runtime values in compile-time context.
- `IntegralTemplatedTuple`: Create a tuple-like structure that instantiates a template for a range of values.

## Usage

### `switch_cases`
Use runtime values in compile-time context. This is realised by instantiating ranges of values at compile-time and dispatching to the correct version at runtime. You can add fallbacks for when the runtime value lies outside the range defined. By using `switch_cases` inside of `switch_cases` multidimensional ranges can be handled as well. Examples can be found [here](examples/examples_SwitchTemplateFunctions.cpp).

### `IntegralTemplatedTuple`
Create a tuple-like structure that instantiates a template for a range of values. Let's say you have a type like
```cpp
template <std::size_t N> struct Type{...};
```
Then you can create a tuple consisting of `Type<i>, Type<i+1>, ...` up to `Type<j>` for `i<=j` with this code. Negative indices, recasting to fewer values and non-default construction is also possible. Examples can be found [here](examples/examples_IntegralTemplatedTuple.cpp).

### Further Examples

Compilable code examples can be found in [examples](./examples). The example build requires the cmake option `-DBUILD_EXAMPLES=ON` to be added.

## Requirements

A C++20 compatible compiler. Code was only tested on x86_64.

## Include it into your projects

### CMake

add
```cmake
FetchContent_Declare(
        DiceTemplateLibrary
        GIT_REPOSITORY "https://github.com/dice-group/DiceTemplateLibrary.git"
        GIT_TAG 0.1.0
        GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(DiceTemplateLibrary)
```

to your CMakeLists.txt

You can now add it to your target with:
```cmake
target_link_libraries(your_target
        DiceTemplateLibrary::DiceTemplateLibrary
        )
```

### conan
You can use it with [conan](https://conan.io/).
To do so, you need to add `DiceTemplateLibrary/0.1.0` to the `[requires]` section of your conan file.

## Build and Run Tests and Examples

```shell
# get it 
git clone https://github.com/dice-group/DiceTemplateLibrary.git
cd DiceTemplateLibrary
# build it
mkdir build
cd build
cmake -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON ..
make -j$(nproc)
# run tests
make run_tests
# run examples
./examples/examples_IntegralTemplatedTuple
./examples/examples_SwitchTemplateFunctions
```

