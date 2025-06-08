# Dice Template Library

This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty
handy.
It contains:

- `switch_cases`: Use runtime values in compile-time context.
- `integral_template_tuple`: Create a tuple-like structure that instantiates a template for a range of values.
- `integral_template_variant`: A wrapper type for `std::variant` guarantees to only contain variants of the form `T<ix>` where $\texttt{ix}\in [\texttt{first},\texttt{last}]$ (inclusive).
- `for_{types,values,range}`: Compile time for loops for types, values or ranges
- `polymorphic_allocator`: Like `std::pmr::polymorphic_allocator` but with static dispatch
- `limit_allocator`: Allocator wrapper that limits the amount of memory that is allowed to be allocated
- `DICE_MEMFN`: Macro to pass member functions like free functions as argument. 
- `pool` & `pool_allocator`: Arena/pool allocator optimized for a limited number of known allocation sizes.
- `DICE_DEFER`/`DICE_DEFER_TO_SUCCES`/`DICE_DEFER_TO_FAIL`: On-the-fly RAII for types that do not support it natively (similar to go's `defer` keyword)
- `overloaded`: Composition for `std::variant` visitor lambdas
- `flex_array`: A combination of `std::array`, `std::span` and a `vector` with small buffer optimization
- `tuple_algorithms`: Some algorithms for iterating tuples
- `generator`: The reference implementation of `std::generator` from [P2502R2](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2502r2.pdf)
- `channel`: A single producer, single consumer queue
- `variant2`: Like `std::variant` but optimized for exactly two types
- `mutex`/`shared_mutex`: Rust inspired mutex interfaces that hold their data instead of living next to it
- `static_string`: A string type that is smaller than `std::string` for use cases where you do not need to resize the string

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

### `polymorphic_allocator`
A `std::pmr::polymorphic_allocator`-like type that uses static dispatch instead of dynamic dispatch to choose the allocator.
This allocator is primarily useful for situations where you have inhomogeneous memory and one of the memory
types does not allow dynamic dispatch using vtables; but you still want to mix and match values from both memory types.

For example, you might have some allocations in persistent or shared memory (or generally: memory-mapped allocations) and others on the heap.
The problem with `mmap` allocations is that they will be placed at an arbitrary position in virtual memory each time they are loaded,
therefore absolute pointers will cause segfaults if the segment is reloaded.
Which means: vtables will not work (because they use absolute pointers) and therefore you cannot use `std::pmr::polymorphic_allocator`.

### `limit_allocator`
Allocator wrapper that limits the amount of memory that can be allocated through the inner allocator.
If the limit is exceeded it will throw `std::bad_alloc`.

### `DICE_MEMFN`
MEMFN is a convenience macro that makes it easy to pass member functions as argument, e.g., to range adaptors.
It eliminates boilerplate code by creating a lambda that captures this and perfectly forwards
arguments to your member function.

### `pool_allocator`
A memory arena/pool allocator with configurable allocation sizes. This is implemented
as a collection of pools with varying allocation sizes. Allocations that do not
fit into any of its pools are directly served via `new`.

### `DICE_DEFER`/`DICE_DEFER_TO_SUCCES`/`DICE_DEFER_TO_FAIL`
A mechanism similar to go's `defer` keyword, which can be used to defer some action to scope exit.
The primary use-case for this is on-the-fly RAII-like resource management for types that do not support RAII (for example C types).
Usage examples can be found [here](examples/examples_defer.cpp).


### `tuple algorthims`
Some algorithms for iterating tuples, for example `tuple_fold` a fold/reduce implementation for tuples.

### `flex_array`
A combination of `std::array`, `std::span` and a `vector` with small buffer optimization where the size is either
statically known or a runtime variable depending on the `extent`/`max_extent` template parameters

### `generator`
The reference implementation of `std::generator` from [P2502R2](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2502r2.pdf).
By default, the generator and corresponding utilities are exported under the `dice::template_library::` namespace.
If you want this generator to serve as a drop in replacement for `std::generator` until it arrives
use `#define DICE_TEMPLATELIBRARY_GENERATOR_STD_COMPAT 1` before including the generator header. That will export
all generator related things under namespace `std::`.

### `channel`
A single producer, single consume queue. This can be used to communicate between threads in a more high level
fashion than a mutex+container would allow.

### `variant2`
Like `std::variant` but specifically optimized for usage with two types/variants. 
The internal representation is a `union` of the two types plus a 1 byte (3 state) discriminant.
Additionally, `visit` does not involve any virtual function calls.

### `type_traits.hpp`
Things that are missing in the standard library `<type_traits>` header.

### `mutex`/`shared_mutex`
Rust inspired mutex interfaces that hold their data instead of living next to it.
The benefit of this approach is that it makes it harder (impossible in rust) to access the
data without holding the mutex.

### `static_string`
A string type that is smaller than `std::string` but does not have the ability to grow or shrink.
This is useful if you never need to resize the string and want to keep the memory footprint low.
It also supports allocators with "fancy" pointers.

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
        GIT_TAG v1.9.1
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
To do so, you need to add `dice-template-library/1.9.1` to the `[requires]` section of your conan file.

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

