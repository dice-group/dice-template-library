////////////////////////////////////////////////////////////////
// Tests adapted from reference implementation of std::generator proposal P2502R2
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2502r2.pdf

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>
#include <type_traits>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <dice/template-library/generator.hpp>

TEST_SUITE("generator") {
	using namespace DICE_TEMPLATELIBRARY_GENERATOR_NAMESPACE;

	///////////////////////
    // Simple non-nested serial generator

    generator<uint64_t> fib(int const max) {
        auto a = 0, b = 1;
        for (auto n = 0; n < max; n++) {
            co_yield std::exchange(a, std::exchange(b, a + b));
        }
    }

    generator<int> other_generator(int i, int const j) {
        while (i != j) {
            co_yield i++;
        }
    }

    /////////////////////
    // Demonstrate ability to yield nested sequences
    //
    // Need to use std::ranges::elements_of() to trigger yielding elements of
    // nested sequence.
    //
    // Supports yielding same generator type (with efficient resumption for
    // recursive cases)
    //
    // Also supports yielding any other range whose elements are convertible to
    // the current generator's elements.

    generator<uint64_t, uint64_t> nested_sequences_example() {
        std::printf("yielding elements_of std::array\n");
#if defined(__GNUC__) && !defined(__clang__)
        co_yield ranges::elements_of(std::array<const uint64_t, 5>{2, 4, 6, 8, 10}, {});
#else
        co_yield ranges::elements_of{std::array<const uint64_t, 5>{2, 4, 6, 8, 10}};
#endif
        std::printf("yielding elements_of nested std::generator\n");
        co_yield ranges::elements_of{fib(10)};

        std::printf("yielding elements_of other kind of generator\n");
        co_yield ranges::elements_of{other_generator(5, 8)};
    }

    //////////////////////////////////////
    // Following examples show difference between:
    //
    //                                    If I co_yield a...
    //                              X / X&&  | X&         | const X&
    //                           ------------+------------+-----------
    // - generator<X, X>               (same as generator<X, X&&>)
    // - generator<X, const X&>   ref        | ref        | ref
    // - generator<X, X&&>        ref        | ill-formed | ill-formed
    // - generator<X, X&>         ill-formed | ref        | ill-formed

    struct X {
        int id;
        X(int const id) : id(id) {
            std::printf("X::X(%i)\n", id);
        }
        X(const X& x) : id(x.id) {
            std::printf("X::X(copy %i)\n", id);
        }
        X(X&& x) : id(std::exchange(x.id, -1)) {
            std::printf("X::X(move %i)\n", id);
        }
        ~X() {
            std::printf("X::~X(%i)\n", id);
        }
    };

    generator<X> always_ref_example() {
        co_yield X{1};
        {
            X x{2};
            co_yield x;
            assert(x.id == 2);
        }
        {
            const X x{3};
            co_yield x;
            assert(x.id == 3);
        }
        {
            X x{4};
            co_yield std::move(x);
        }
    }

    generator<X&&> xvalue_example() {
        co_yield X{1};
        X x{2};
        co_yield x; // well-formed: generated element is copy of lvalue
        assert(x.id == 2);
        co_yield std::move(x);
    }

    generator<const X&> const_lvalue_example() {
        co_yield X{1}; // OK
        const X x{2};
        co_yield x; // OK
        co_yield std::move(x); // OK: same as above
    }

    generator<X&> lvalue_example() {
        // co_yield X{1}; // ill-formed: prvalue -> non-const lvalue
        X x{2};
        co_yield x; // OK
        // co_yield std::move(x); // ill-formed: xvalue -> non-const lvalue
    }

    ///////////////////////////////////
    // These examples show different usages of reference/value_type
    // template parameters

    // value_type = std::unique_ptr<int>
    // reference = std::unique_ptr<int>&&
    generator<std::unique_ptr<int>&&> unique_ints(const int high) {
        for (auto i = 0; i < high; ++i) {
            co_yield std::make_unique<int>(i);
        }
    }

    // value_type = std::string_view
    // reference = std::string_view&&
    generator<std::string_view> string_views() {
        co_yield "foo";
        co_yield "bar";
    }

    // value_type = std::string
    // reference = std::string_view
    template <typename Allocator>
    generator<std::string_view, std::string> strings(std::allocator_arg_t, Allocator) {
        co_yield {};
        co_yield "start";
        for (auto sv : string_views()) {
            co_yield std::string{sv} + '!';
        }
        co_yield "end";
    }

    // Resulting vector is deduced using ::value_type.
    template <std::ranges::input_range R>
    std::vector<std::ranges::range_value_t<R>> to_vector(R&& r) {
        std::vector<std::ranges::range_value_t<R>> v;
        for (auto&& x : r) {
            v.emplace_back(static_cast<decltype(x)&&>(x));
        }
        return v;
    }

    // zip() algorithm produces a generator of tuples where
    // the reference type is a tuple of references and
    // the value type is a tuple of values.
    template <std::ranges::range... Rs, std::size_t... Indices>
    generator<std::tuple<std::ranges::range_reference_t<Rs>...>,
        std::tuple<std::ranges::range_value_t<Rs>...>>
        zip_impl(std::index_sequence<Indices...>, Rs... rs) {
        std::tuple<std::ranges::iterator_t<Rs>...> its{std::ranges::begin(rs)...};
        std::tuple<std::ranges::sentinel_t<Rs>...> itEnds{std::ranges::end(rs)...};
        while (((std::get<Indices>(its) != std::get<Indices>(itEnds)) && ...)) {
            co_yield {*std::get<Indices>(its)...};
            (++std::get<Indices>(its), ...);
        }
    }

    template <std::ranges::range... Rs>
    generator<std::tuple<std::ranges::range_reference_t<Rs>...>,
        std::tuple<std::ranges::range_value_t<Rs>...>>
        zip(Rs&&... rs) {
        return zip_impl(std::index_sequence_for<Rs...>{}, std::views::all(std::forward<Rs>(rs))...);
    }

    template <typename T>
    struct stateful_allocator {
        using value_type = T;

        int id;

        explicit stateful_allocator(int const id) noexcept : id(id) {}

        template <typename U>
        stateful_allocator(const stateful_allocator<U>& x) : id(x.id) {}

        T* allocate(std::size_t count) {
            std::printf("stateful_allocator(%i).allocate(%zu)\n", id, count);
            return std::allocator<T>().allocate(count);
        }

        void deallocate(T* ptr, std::size_t count) noexcept {
            std::printf("stateful_allocator(%i).deallocate(%zu)\n", id, count);
            std::allocator<T>().deallocate(ptr, count);
        }

        template <typename U>
        bool operator==(const stateful_allocator<U>& x) const {
            return this->id == x.id;
        }
    };

    generator<int, void, std::allocator<std::byte>> stateless_example() {
        co_yield 42;
    }

    generator<int, void, std::allocator<std::byte>> stateless_example(
        std::allocator_arg_t, std::allocator<std::byte>) {
        co_yield 42;
    }

    template <typename Allocator>
    generator<int, void, Allocator> stateful_alloc_example(std::allocator_arg_t, Allocator) {
        co_yield 42;
    }

    struct member_coro {
        generator<int> f() const {
            co_yield 42;
        }
    };

	TEST_CASE("nested_sequences") {
		std::vector<uint64_t> const expected{2, 4, 6, 8, 10, 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 5, 6, 7};
		std::vector<uint64_t> actual;

		for (auto x : nested_sequences_example()) {
			actual.push_back(x);
		}

		REQUIRE_EQ(actual, expected);
	}

	TEST_CASE("by_value") {
		SUBCASE("fib") {
			std::vector<uint64_t> const expected{0, 1, 1, 2, 3};
			std::vector<uint64_t> actual;

			for (auto x : fib(5)) {
				actual.push_back(x);
			}

			REQUIRE_EQ(actual, expected);
		}

		SUBCASE("seq") {
			std::vector<uint64_t> const expected{1, 2, 3, 4};
			std::vector<uint64_t> actual;

			for (auto x : other_generator(1, 5)) {
				actual.push_back(x);
			}

			REQUIRE_EQ(actual, expected);
		}
	}

	TEST_CASE("always_ref") {
		std::vector<int> const expected{1, 2, 3, 4};
		std::vector<int> actual;

		for (auto&& x : always_ref_example()) {
			actual.push_back(x.id);
		}

		REQUIRE_EQ(actual, expected);
	}

	TEST_CASE("by_rvalue_ref") {
		std::vector<int> const expected{1, 2, 2};
		std::vector<int> actual;

		for (auto&& x : xvalue_example()) {
			static_assert(std::is_rvalue_reference_v<decltype(x)>);
			static_assert(!std::is_const_v<std::remove_reference_t<decltype(x)>>);
			actual.push_back(x.id);
		}

		REQUIRE_EQ(actual, expected);
	}

	TEST_CASE("by_const_ref") {
		std::vector<int> const expected{1, 2, 2};
		std::vector<int> actual;

		for (auto&& x : const_lvalue_example()) {
			static_assert(std::is_lvalue_reference_v<decltype(x)>);
			static_assert(std::is_const_v<std::remove_reference_t<decltype(x)>>);
			actual.push_back(x.id);
		}

		REQUIRE_EQ(actual, expected);
	}

	TEST_CASE("by_lvalue_ref") {
		std::vector<int> const expected{2};
		std::vector<int> actual;

		for (auto&& x : lvalue_example()) {
			static_assert(std::is_lvalue_reference_v<decltype(x)>);
			static_assert(!std::is_const_v<std::remove_reference_t<decltype(x)>>);
			actual.push_back(x.id);
		}

		REQUIRE_EQ(actual, expected);
	}

	TEST_CASE("value_type") {
		SUBCASE("string_views") {
			std::vector<std::string_view> const s1_expected{"foo", "bar"};
			std::vector<std::string_view> const s1_actual = to_vector(string_views());
			REQUIRE_EQ(s1_actual, s1_expected);
		}

		SUBCASE("strings") {
			std::vector<std::string> const s2_expected{"", "start", "foo!", "bar!", "end"};
			std::vector<std::string> const s2_actual = to_vector(strings(std::allocator_arg, std::allocator<std::byte>{}));
			REQUIRE_EQ(s2_actual, s2_expected);
		}

		SUBCASE("string tuples") {
			std::vector<std::tuple<std::string, std::string>> const s3_expected{{"", ""}, {"start", "start"}, {"foo!", "foo!"}, {"bar!", "bar!"}, {"end", "end"}};
			std::vector<std::tuple<std::string, std::string>> const s3_actual =
				to_vector(zip(strings(std::allocator_arg, std::allocator<std::byte>{}), strings(std::allocator_arg, std::allocator<std::byte>{})));

			REQUIRE_EQ(s3_actual, s3_expected);
		}
	}

	TEST_CASE("move_only") {
		std::vector<int> const expected{0, 1, 2, 3, 4};
		std::vector<int> actual;

		for (std::unique_ptr<int> ptr : unique_ints(5)) {
			actual.push_back(*ptr);
		}

		REQUIRE_EQ(actual, expected);
	}

	TEST_CASE("stateless_alloc") {
		SUBCASE("a") {
			auto g = stateless_example();
			REQUIRE_EQ(*g.begin(), 42);
		}

		SUBCASE("b") {
			auto g = stateless_example(std::allocator_arg, std::allocator<float>{});
			REQUIRE_EQ(*g.begin(), 42);
		}
	}

	TEST_CASE("stateful_alloc") {
		auto g = stateful_alloc_example(std::allocator_arg, stateful_allocator<double>{42});
		REQUIRE_EQ(*g.begin(), 42);
	}

	TEST_CASE("member_coro") {
		member_coro m;
		REQUIRE_EQ(*m.f().begin(), 42);
	}
}
