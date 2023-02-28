# Dice Template Library

This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty
handy.
It contains:

- `switch_cases`: Use runtime values in compile-time context.
- `integral_template_tuple`: Create a tuple-like structure that instantiates a template for a range of values.
- `integral_template_variant`: A wrapper type for `std::variant` guarantees to only contain variants of the form `T<IX>` where $\texttt{IX}\in [\texttt{FIRST},\texttt{LAST}]$ (inclusive).
- `for_{types,values,range}`: Compile time for loops for types, values or ranges

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

### `integral_template_variant`

Creates a variant-like structure that instantiates a template for a range of values. Let's say you have a type like
```cpp
template <std::size_t N> struct my_type{...};
```

Then you can create a variant consisting of `my_type<i>, my_type<i+1>, ..., my_type<j>` with the help of `integral_template_variant<my_type, i, j>`.
Negative indices, and j <= i are also possible. Examples can be
found [here](examples/examples_integral_template_variant.cpp).

### `for_{types,values,range}`

Different flavors of compile time loops that allow to iterate types, values or ranges at compile time. Types and values are provided as template arguments and a lambda to be called for each of them is passed as function argument, e.g. `for_types<uint8_t, uint64_t>([]<typename T>() {})` and `for_values<1, 1.1, 'c'>([](auto x) {})`. Ranges are defined by template parameters for start and exclusive end and receive a function to be applied to each range element as function argument, e.g. `for_range<3, 5>([](auto x) {})`, including support for decreasing ranges and negative indices, e.g. `for_range<2, -4>([](auto x) {})`. Examples can
be found [here](examples/examples_for.cpp).

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
        GIT_TAG v0.3.1
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
To do so, you need to add `dice-template-library/0.3.0` to the `[requires]` section of your conan file.

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

