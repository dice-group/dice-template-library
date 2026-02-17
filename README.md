# Dice Template Library

This template library is a collection of template-oriented code that we, the Data Science Group at UPB, found pretty
handy.
It contains:

- `switch_cases`: Use runtime values in compile-time context.
- `integral_template_tuple`: Create a tuple-like structure that instantiates a template for a range of values.
- `integral_template_variant`: A wrapper type for `std::variant` guarantees to only contain variants of the form `T<ix>` where ix in [first, last) (inclusive, exclusive).
- `integral_sequence`: Utilities for generating compile-time integer sequences with automatic direction detection.
- `for_{types,values,range}`: Compile time for loops for types, values or ranges
- `polymorphic_allocator`: Like `std::pmr::polymorphic_allocator` but with static dispatch
- `limit_allocator`: Allocator wrapper that limits the amount of memory that is allowed to be allocated
- `DICE_MEMFN`: Macro to pass member functions like free functions as argument. 
- `pool` & `pool_allocator`: Arena/pool allocator optimized for a limited number of known allocation sizes.
- `DICE_DEFER`/`DICE_DEFER_TO_SUCCES`/`DICE_DEFER_TO_FAIL`: On-the-fly RAII for types that do not support it natively (similar to go's `defer` keyword)
- `overloaded` and `match`: Batteries for `std::variant` (and also `dtl::variant2`). Compose re-usable visitors with `overload` or apply a single-use visitor directly with `match`.
- `flex_array`: A combination of `std::array`, `std::span` and a `vector` with small buffer optimization
- `tuple_algorithms`: Some algorithms for iterating tuples
- `fmt_join`: A helper to join elements of a range with a separator for use with `std::format` alike [fmt::join](https://fmt.dev/latest/api/#range-and-tuple-formatting)
- `channel`: A single producer, single consumer queue
- `variant2`: Like `std::variant` but optimized for exactly two types
- `mutex`/`shared_mutex`: Rust inspired mutex interfaces that hold their data instead of living next to it
- `static_string`: A string type that is smaller than `std::string` for use cases where you do not need to resize the string
- `ranges`: Additional range algorithms and adaptors that are missing from the standard library.  
- `next_to_range`/`next_to_view`/`next_to_iter`: Eliminate the boilerplate required to write C++ iterators and ranges.
- `inplace_polymorphic`: `std::variant`-like on-stack polymorphism based on `virtual` functions.
- `type_list`: A variadic lists of types for metaprogramming.
- `lazy_conditional`: Lazy conditional type selection that only instantiates the selected branch.
- `format_to_ostream`: Provide an ostream `operator<<` overload for any type that is formattable with `std::format`.
- `stdint`: User defined literals for fixed size integers.
- `functional`: Extensions for `<functional>`. Currently, contains a `bind_front` implementation with constexpr function argument.
- `DICE_DBG`: Prints and returns the value of a given expression for quick and dirty debugging.

## Usage

### `switch_cases`

Use runtime values in a compile-time context. This is realised by instantiating ranges of values at compile-time and
dispatching to the correct version at runtime. You can add fallbacks for when the runtime value lies outside the range
defined. By using `switch_cases` inside of `switch_cases` multidimensional ranges can be handled as well. Examples can
be found [here](examples/examples_switch_cases.cpp).

### `integral_template_tuple`

Create a tuple-like structure that instantiates a template for a range of values. Let's say you have a type like

```cpp
template <std::size_t N> struct my_type{...};
```

Then you can create a tuple consisting of `my_type<i>, my_type<i+1>, ...` up to `my_type<j - 1>` for `i < j` with this code.
Negative indices, recasting to fewer values and non-default construction are also possible.

**Automatic direction detection** is used based on the relationship between `first` and `last`:

- `integral_template_tuple<i, j, my_type>` creates:
  - `my_type<i>, my_type<i+1>, ..., my_type<j-1>` when `i < j` (ascending, exclusive upper bound `[i, j)`)
  - `my_type<i>, my_type<i-1>, ..., my_type<j+1>` when `i > j` (descending, exclusive lower bound `(j, i]`)
  - Empty tuple when `i == j`
    Examples can be found [here](examples/examples_integral_template_tuple.cpp).

### `integral_template_variant`

Creates a variant-like structure that instantiates a template for a range of values. Let's say you have a type like

```cpp
template <std::size_t N> struct my_type{...};
```

Then you can create a variant consisting of `my_type<i>, my_type<i+1>, ..., my_type<j - 1>` with the help of
`integral_template_variant<i, j, my_type>`.
Negative indices and both ascending and descending ranges are supported.

**Automatic direction detection** is used based on the relationship between `first` and `last`:

- `integral_template_variant<i, j, my_type>` creates a variant of:
  - `my_type<i>, my_type<i+1>, ..., my_type<j-1>` when `i < j` (ascending, exclusive upper bound `[i, j)`)
  - `my_type<i>, my_type<i-1>, ..., my_type<j+1>` when `i > j` (descending, exclusive lower bound `(j, i]`)
  - Empty variant when `i == j`

Examples can be found [here](examples/examples_integral_template_variant.cpp).

### `integral_sequence`

Provides utilities for working with compile-time integer sequences. This is the foundation for
`integral_template_tuple` and `integral_template_variant`.

- `make_integer_sequence<Int, first, last>`: Generate `std::integer_sequence` with automatic direction detection
- `make_index_sequence<first, last>`: Convenience wrapper for `std::integer_sequence<size_t, X, Y>`
- `make_integral_constant_list<Int, first, last>`: Generate `type_list` of `std::integral_constant<Int, ix>`

All utilities automatically determine direction:

- `first == last`: empty sequence
- `first < last`: ascending `[first, last)` (exclusive upper bound)
- `first > last`: descending `(last, first]` (exclusive lower bound)

Examples can be found [here](examples/example_integral_sequence.cpp).

### `for_{types,values,range}`

Different flavors of compile time loops that allow to iterate types, values or ranges at compile time. Types and values are provided as template arguments and a lambda to be called for each of them is passed as function argument, e.g. `for_types<uint8_t, uint64_t>([]<typename T>() {})` and `for_values<1, 1.1, 'c'>([](auto x) {})`. Ranges are defined by template parameters for start and exclusive end and receive a function to be applied to each range element as function argument, e.g. `for_range<3, 5>([](auto x) {})`, including support for decreasing ranges and negative indices, e.g. `for_range<2, -4>([](auto x) {})`. Examples can
be found [here](examples/examples_for.cpp).

### `polymorphic_allocator`
A `std::pmr::polymorphic_allocator`-like type that uses static dispatch instead of dynamic dispatch to choose the allocator.
This allocator is primarily useful for situations where you have inhomogeneous memory, and one of the memory
types does not allow dynamic dispatch using vtables; but you still want to mix and match values from both memory types.

For example, you might have some allocations in persistent or shared memory (or generally: memory-mapped allocations) and others on the heap.
The problem with `mmap` allocations is that they will be placed at an arbitrary position in virtual memory each time they are loaded,
therefore, absolute pointers will cause segfaults if the segment is reloaded.
Which means: vtables will not work (because they use absolute pointers) and therefore you cannot use `std::pmr::polymorphic_allocator`.

### `limit_allocator`
Allocator wrapper that limits the amount of memory that can be allocated through the inner allocator.
If the limit is exceeded it will throw `std::bad_alloc`.

### `DICE_MEMFN`
DICE_MEMFN is a convenience macro that makes it easy to pass member functions as argument, e.g., to range adaptors.
It eliminates boilerplate code by creating a lambda that captures this and perfectly forwards
arguments to your member function.

### `pool_allocator`
A memory arena/pool allocator with configurable allocation sizes. This is implemented
as a collection of pools with varying allocation sizes. Allocations that do not
fit into any of its pools are directly served via `new`.

### `DICE_DEFER`/`DICE_DEFER_TO_SUCCES`/`DICE_DEFER_TO_FAIL`
A mechanism similar to go's `defer` keyword, which can be used to defer some action to scope exit.
The primary use-case for this is on-the-fly RAII-like resource management for types that do not support RAII (for example, C types).
Usage examples can be found [here](examples/examples_defer.cpp).

### `tuple algorithms`
Some algorithms for iterating tuples, for example `tuple_fold` a fold/reduce implementation for tuples.

### `fmt_join`
Works just like [`fmt::join`](https://fmt.dev/latest/api/#range-and-tuple-formatting) but for `std::format`.

### `flex_array`
A combination of `std::array`, `std::span` and a `vector` with small buffer optimization where the size is either
statically known or a runtime variable depending on the `extent`/`max_extent` template parameters

### `channel`
A single-producer, single-consume queue. This can be used to communicate between threads in a more high level
fashion than a mutex+container would allow.

### `variant2`
Like `std::variant` but specifically optimized for usage with two types/variants. 
The internal representation is a `union` of the two types plus a 1 byte (3 state) discriminant.
Additionally, `visit` does not involve any virtual function calls.

### `overload` / `match`
Things that are missing around `std::variant` and visitors in the standard library. Implementation of the common `overload` pattern to create re-usable visitors. Also, comes with a `match` function that allows you to declare the visitor directly inline when applying it. 

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

### `ranges`
Additional range algorithms (e.g. `unique_view`) and adaptors (e.g., a pipeable `all_of`)
that are missing from the standard library.

### `next_to_range`/`next_to_view`/`next_to_iter`
Eliminate the boilerplate required to write C++ iterators and ranges.
To get a fully functional range, the only thing that is required is
implementing a minimal, rust-style iterator interface.

#### Limitations
Due to technical reasons in how `std::ranges::range`s are specified, `next_to_view` is **not** suitable
to build truly general purpose "transformer"/pipeline views (i.e. ranges that take existing ranges and do stuff to them).

It is possible to build "transformer" views, but you need to limit yourself to copyable views inside of them, i.e. you cannot use
`std::ranges::owning_view` (which is only movable), you must use `std::ranges::ref_view`. Therefore, it would not be possible to
move a vector inside a pipeline that has a `next_to_view`-based view as part of it.
(It works fine if you don't **move** the vector into the pipeline.)

### `inplace_polymorphic`
Similar to `std::variant`, this type allows for polymorphism without heap allocations.
Unlike `std::variant`, it uses polymorphism based on `virtual` functions instead of `std::visit`/`std::get`.
It is meant as variant-like stack-storage for similar types (i.e. types in an inheritance hierarchy).
This greatly reduces the boilerplate compared to `std::visit` based on-stack polymorphism.

### `type_list`
A variadic list of types for use in metaprogramming. Optimized to be much faster than the equivalent code
written with `std::tuple`. It supports various operations similar to what you would use for
`std::ranges` (e.g. `transform`, `filter`, etc.).

### `lazy_conditional`
A lazy alternative to `std::conditional` that only instantiates the selected branch.
Unlike `std::conditional`, which eagerly evaluates both branches, `lazy_conditional` only accesses
the `::type` member of the branch that is selected. This prevents compilation errors when
the non-selected branch would be ill-formed (e.g., contains a `static_assert(false)` or
accesses invalid type members).

Additionally, `lazy_switch` provides multi-way conditional type selection using a first-match
approach with `case_<bool, Provider>` helpers. Only the selected case's provider is instantiated,
allowing you to use `static_assert(false)` in default cases that should never be reached.
Examples can be found [here](examples/examples_lazy_conditional.cpp).


### `format_to_ostream`
Provide an ostream `operator<<` overload for any type that is formattable with `std::format`.
Does not override preexisting `operator<<` implementations.

The primary usage for this is for doctest tests, because doctest only supports output via `std::ostream` (not `std::format`).
Note: for this to work it needs to be included **before** `<doctest/doctest.h>`.


### `stdint`
User defined literals for fixed size integers (e.g. `123_u64`).
This is mainly useful for cross-platform applications where the common `123ul` is not always the same as `uint64_t{123}`.
For instance, on macOS `uint64_t` is defined as `unsigned long long`, whereas on Linux it is defined as `unsigned long`.
Even if both `unsigned long` and `unsigned long long` have the same size, they are still distinct types which can cause issues
when a type is being deduced.

### `functional`
Extensions for `<functional>`.
Currently, contains an implementation of bind_front with constexpr function argument (`bind_front<constexpr_func>(args...)`)
that is only available from C++26 onwards.

### `DICE_DBG`
A macro for debugging inspired by rust's `dbg!` macro.
It prints and returns the value of a given expression.


### Further Examples

Compilable code examples can be found in [examples](./examples). The example build requires the cmake
option `-DBUILD_EXAMPLES=ON` to be added.

## Requirements

A C++23 compatible compiler. Code was only tested on x86_64.

## Include it in your projects
### Conan
You can use it with [conan](https://conan.io/).
To do so, you need to add `dice-template-library/2.0.2` to the `[requires]` section of your conan file.

## Build and Run Tests and Examples

```shell
# get it 
git clone https://github.com/dice-group/dice-template-library.git
cd dice-template-library

wget https://raw.githubusercontent.com/conan-io/cmake-conan/refs/heads/develop2/conan_provider.cmake

# build it
mkdir build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=conan_provider.cmake -B build .
cmake --build build --parallel $(nproc)

# run tests
cd build
ctest

# run an example
cd build
./examples/examples_dbg
```
