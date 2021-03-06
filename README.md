# Dice Template Library

This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty
handy.
It contains:

- `switch_cases`: Use runtime values in compile-time context.
- `integral_template_tuple`: Create a tuple-like structure that instantiates a template for a range of values.

## Usage

### `switch_cases`

Use runtime values in compile-time context. This is realised by instantiating ranges of values at compile-time and
dispatching to the correct version at runtime. You can add fallbacks for when the runtime value lies outside the range
defined. By using `switch_cases` inside of `switch_cases` multidimensional ranges can be handled as well. Examples can
be found [here](examples/examples_switch_cases.cpp).

### `integral_template_tuple`

Create a tuple-like structure that instantiates a template for a range of values. Let's say you have a type like

```cpp
template <std::size_t N> struct my_type{...};
```

Then you can create a tuple consisting of `my_type<i>, my_type<i+1>, ...` up to `my_type<j>` for `i<=j` with this code.
Negative indices, recasting to fewer values and non-default construction is also possible. Examples can be
found [here](examples/examples_integral_template_tuple.cpp).

### `for_{types,values,range}`

Different flavors of compile time loops that allow to iterate types, values or ranges at compile time.

### Further Examples

Compilable code examples can be found in [examples](./examples). The example build requires the cmake
option `-DBUILD_EXAMPLES=ON` to be added.

## Requirements

A C++20 compatible compiler. Code was only tested on x86_64.

## Include it into your projects

### CMake

add

```cmake
FetchContent_Declare(
        dice-template-library
        GIT_REPOSITORY "https://github.com/dice-group/dice-template-library.git"
        GIT_TAG v0.2.0
        GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(dice-template-library)
```

to your CMakeLists.txt

You can now add it to your target with:

```cmake
target_link_libraries(your_target
        dice-template-library::dice-template-library
        )
```

### conan

You can use it with [conan](https://conan.io/).
To do so, you need to add `dice-template-library/0.2.0` to the `[requires]` section of your conan file.

## Build and Run Tests and Examples

```shell
# get it 
git clone https://github.com/dice-group/dice-template-library.git
cd dice-template-library
# build it
mkdir build
cd build
cmake -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON ..
make -j$(nproc)
# run tests
make run_tests
# run examples
./examples/examples_integral_template_tuple
./examples/examples_switch_cases
```

