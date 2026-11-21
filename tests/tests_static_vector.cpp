#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/static_vector.hpp>

#include <algorithm>
#include <atomic>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <vector>

TEST_SUITE("static_vector") {
	using namespace dice::template_library;

	static_assert(sizeof(static_vector<int>) == 2 * sizeof(void *));
	static_assert(std::ranges::contiguous_range<static_vector<int>>);
	static_assert(std::ranges::sized_range<static_vector<int>>);
	static_assert(!std::three_way_comparable<static_vector<std::vector<int>::allocator_type>>);

	/// An allocator that is never equal and does not propagate, this forces the element-wise move assignment
	template<typename T>
	struct weird_allocator : std::allocator<T> {
		using is_always_equal = std::false_type;
		using propagate_on_container_move_assignment = std::false_type;

		bool operator==([[maybe_unused]] weird_allocator const &other) const noexcept {
			return false;
		}
	};

	template<typename T, typename A>
	void check_empty(static_vector<T, A> const &v) {
		REQUIRE_EQ(v.data(), nullptr);
		REQUIRE_EQ(v.size(), 0);
		REQUIRE(v.empty());
		REQUIRE_EQ(v.begin(), v.end());
		REQUIRE_EQ(v.cbegin(), v.cend());
		REQUIRE_EQ(v.rbegin(), v.rend());
		REQUIRE_EQ(v.crbegin(), v.crend());
		REQUIRE_EQ(v, static_vector<T, A>{});
	}

	template<typename T, typename A>
	void check_non_empty(static_vector<T, A> const &actual, std::vector<T> const &expected) {
		REQUIRE_FALSE(actual.empty());
		REQUIRE_EQ(actual.size(), expected.size());
		REQUIRE(std::ranges::equal(actual, expected));
		REQUIRE(std::ranges::equal(actual | std::views::reverse, expected | std::views::reverse));
		REQUIRE(std::ranges::equal(actual.cbegin(), actual.cend(), expected.begin(), expected.end()));
		REQUIRE(std::ranges::equal(actual.crbegin(), actual.crend(), expected.rbegin(), expected.rend()));
		REQUIRE_EQ(actual.front(), expected.front());
		REQUIRE_EQ(actual.back(), expected.back());
		REQUIRE_EQ(actual[0], expected[0]);
		REQUIRE_EQ(actual, actual);
		REQUIRE_EQ(actual <=> actual, std::strong_ordering::equal);
	}

	TEST_CASE("empty vector") {
		check_empty(static_vector<int>{});
		check_empty(static_vector<int>(0));
		check_empty(static_vector<int>(0, 42));
		check_empty(static_vector<int>(std::initializer_list<int>{}));

		std::vector<int> const empty;
		check_empty(static_vector<int>(empty.begin(), empty.end()));
	}

	TEST_CASE("ctors") {
		check_non_empty(static_vector<int>(3), {0, 0, 0});
		check_non_empty(static_vector<int>(3, 42), {42, 42, 42});
		check_non_empty(static_vector<int>{1, 2, 3}, {1, 2, 3});
		check_non_empty(static_vector<std::string>{"Hello", "World"}, {"Hello", "World"});
		check_non_empty(static_vector<std::string>(for_overwrite, 2), {"", ""});
		check_non_empty(static_vector<int, weird_allocator<int>>(3, 42, weird_allocator<int>{}), {42, 42, 42});

		std::vector<std::string> const expected{"Spherical", "Cow"};
		static_vector const v(expected.begin(), expected.end());
		check_non_empty(v, expected);

		static_vector const v2(expected.begin(), expected.end(), weird_allocator<std::string>{});
		check_non_empty(v2, expected);
	}

	TEST_CASE_TEMPLATE("copy and move", A, std::allocator<std::string>, weird_allocator<std::string>) {
		std::vector<std::string> const expected{"Hello", "World"};
		static_vector<std::string, A> v1(expected.begin(), expected.end());

		SUBCASE("copy ctor") {
			static_vector const v2{v1};
			check_non_empty(v1, expected);
			check_non_empty(v2, expected);
		}

		SUBCASE("move ctor") {
			static_vector const v2{std::move(v1)};
			check_empty(v1);
			check_non_empty(v2, expected);
		}

		SUBCASE("copy assignment") {
			static_vector<std::string, A> v2{"Spherical", "Cow", "Vacuum"};
			v2 = v1;
			check_non_empty(v1, expected);
			check_non_empty(v2, expected);

			v2 = std::as_const(v2);
			check_non_empty(v2, expected);

			v2 = static_vector<std::string, A>{};
			v1 = v2;
			check_empty(v1);
		}

		SUBCASE("move assignment") {
			static_vector<std::string, A> v2{"Spherical", "Cow", "Vacuum"};
			v2 = std::move(v1);
			check_non_empty(v2, expected);
			// the storage is only taken if the allocators are equal
			REQUIRE_EQ(v1.empty(), std::allocator_traits<A>::is_always_equal::value);

			v2 = static_vector<std::string, A>{};
			check_empty(v2);
		}

		SUBCASE("swap") {
			static_vector<std::string, A> v2;
			swap(v1, v2);
			check_empty(v1);
			check_non_empty(v2, expected);
		}
	}

	TEST_CASE("mutation") {
		static_vector<int> v(4);
		v[1] = 1;
		v.front() = 5;
		v.back() = 3;
		*(v.begin() + 2) = 2;
		check_non_empty(v, {5, 1, 2, 3});

		std::ranges::sort(v);
		check_non_empty(v, {1, 2, 3, 5});

		*v.rbegin() = 4;
		check_non_empty(v, {1, 2, 3, 4});
	}

	TEST_CASE("comparison") {
		static_vector<int> const a{1, 2, 3};
		static_vector<int> const b{1, 2, 4};
		static_vector<int> const c{1, 2};

		REQUIRE_NE(a, b);
		REQUIRE_NE(a, c);
		REQUIRE_LT(a, b);
		REQUIRE_GT(a, c);
		REQUIRE_EQ(a <=> b, std::strong_ordering::less);
		REQUIRE_EQ(a <=> c, std::strong_ordering::greater);
	}

	TEST_CASE("std::span") {
		static_vector<int> v{1, 2, 3};
		static_vector<int> const cv{4, 5};

		static_assert(std::same_as<decltype(std::span{v}), std::span<int>>);
		static_assert(std::same_as<decltype(std::span{cv}), std::span<int const>>);
		static_assert(!std::convertible_to<static_vector<int> const &, std::span<int>>);

		std::span<int> const s = v;
		REQUIRE_EQ(s.data(), v.data());
		REQUIRE_EQ(s.size(), v.size());

		s[0] = 42;
		REQUIRE_EQ(v.front(), 42);

		std::span<int const> const cs = cv;
		REQUIRE_EQ(cs.data(), cv.data());
		REQUIRE_EQ(cs.size(), cv.size());
	}

	TEST_CASE("move-only type") {
		static_vector<std::unique_ptr<int>> v1(2);
		v1[0] = std::make_unique<int>(42);

		static_vector const v2{std::move(v1)};
		REQUIRE(v1.empty());
		REQUIRE_EQ(v2.size(), 2);
		REQUIRE_EQ(*v2[0], 42);
		REQUIRE_EQ(v2[1], nullptr);
	}

	TEST_CASE("not default constructible and not movable types") {
		struct no_default {
			int value;
			explicit no_default(int const value) : value{value} {
			}
		};

		static_vector<no_default> const v1(2, no_default{42});
		REQUIRE_EQ(v1.size(), 2);
		REQUIRE_EQ(v1.back().value, 42);

		static_vector<std::atomic<int>> v2(2);
		static_vector<std::atomic<int>> v3;
		v3 = std::move(v2);
		REQUIRE(v2.empty());
		REQUIRE_EQ(v3.size(), 2);
		REQUIRE_EQ(v3.back(), 0);
	}
}
