#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/flex_array.hpp>

#include <algorithm>
#include <compare>
#include <numeric>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

namespace dice::template_library {
	// extern templates sometimes make problems if internal types are too eagerly instantiated
	extern template struct flex_array<int, 5>;
	extern template struct flex_array<int, dynamic_extent, 5>;

	template struct flex_array<int, 5>;
	template struct flex_array<int, dynamic_extent, 5>;
} // namespace dice::template_library

TEST_SUITE("flex_array") {
	using namespace dice::template_library;

	template<size_t extent, size_t max_extent>
	void check_all_static(flex_array<int, extent, max_extent> const &f) {
		REQUIRE_FALSE(f.empty());
		REQUIRE_EQ(f.size(), 5);
		REQUIRE_EQ(f.max_size(), 5);
		REQUIRE_EQ(std::distance(f.begin(), f.end()), 5);
		REQUIRE_EQ(std::distance(f.cbegin(), f.cend()), 5);
		REQUIRE_EQ(std::distance(f.rbegin(), f.rend()), 5);
		REQUIRE_EQ(std::distance(f.crbegin(), f.crend()), 5);
	}

	template<size_t extent, size_t max_extent>
	void check_contents(flex_array<int, extent, max_extent> const &f, size_t expected_size) {
		std::vector<int> ref;
		ref.resize(expected_size);
		std::iota(ref.begin(), ref.end(), 1);

		REQUIRE(std::ranges::equal(f, ref));
		REQUIRE(std::ranges::equal(std::ranges::subrange(f.rbegin(), f.rend()), ref | std::views::reverse)); // using subrange(rbegin(), rend()) just to make really sure that they are used

		if (expected_size > 0) {
			REQUIRE_EQ(*f.data(), 1);
		}

		REQUIRE(std::ranges::equal(ref, std::span{f}));
	}

	template<size_t extent, size_t max_extent>
	void check_all_dynamic(flex_array<int, extent, max_extent> &f, size_t expected_size) {
		REQUIRE_EQ(f.empty(), expected_size == 0);
		REQUIRE_EQ(f.size(), expected_size);
		REQUIRE_EQ(f.max_size(), max_extent);
		REQUIRE_EQ(std::distance(f.begin(), f.end()), expected_size);
		REQUIRE_EQ(std::distance(f.cbegin(), f.cend()), expected_size);
		REQUIRE_EQ(std::distance(f.rbegin(), f.rend()), expected_size);
		REQUIRE_EQ(std::distance(f.crbegin(), f.crend()), expected_size);

		f.resize(5);
		REQUIRE_FALSE(f.empty());
		REQUIRE_EQ(f.size(), 5);
		REQUIRE_EQ(f.max_size(), max_extent);
		REQUIRE_EQ(std::distance(f.begin(), f.end()), 5);
		REQUIRE_EQ(std::distance(f.cbegin(), f.cend()), 5);
		REQUIRE_EQ(std::distance(f.rbegin(), f.rend()), 5);
		REQUIRE_EQ(std::distance(f.crbegin(), f.crend()), 5);

		f[0] = 1;
		f[1] = 2;
		f[2] = 3;
		f[3] = 4;
		f[4] = 5;
		REQUIRE_EQ(std::accumulate(f.begin(), f.end(), 0), 15);
		REQUIRE_EQ(*f.data(), 1);
		REQUIRE_EQ(*(f.data() + f.size() - 1), 5);
	}

	TEST_CASE("static size") {
		static_assert(sizeof(flex_array<int, 1>) == sizeof(int));
		static_assert(alignof(flex_array<int, 1>) == alignof(int));
		static_assert(flex_array<int, 1>::mode == flex_array_mode::direct_static_size);

		SUBCASE("default ctor") {
			flex_array<int, 5> f;
			check_all_static(f);

			f[0] = 1;
			f[1] = 2;
			f[2] = 3;
			f[3] = 4;
			f[4] = 5;
			check_contents(f, 5);
		}

		SUBCASE("init list ctor") {
			flex_array<int, 5> f{1, 2, 3, 4, 5};
			check_all_static(f);
			check_contents(f, 5);
		}

		SUBCASE("iter ctor") {
			std::array<int, 5> ref{1, 2, 3, 4, 5};
			flex_array<int, 5> f(ref.begin(), ref.end());
			check_all_static(f);
			check_contents(f, 5);
		}

		SUBCASE("swap") {
			flex_array<int, 5> f{1, 2, 3, 4, 5};
			flex_array<int, 5> f2{6, 7, 8, 9, 10};

			swap(f, f2);
			REQUIRE(std::ranges::equal(f, std::array{6, 7, 8, 9, 10}));
			REQUIRE(std::ranges::equal(f2, std::array{1, 2, 3, 4, 5}));
		}

		SUBCASE("cmp") {
			flex_array<int, 5> f{1, 2, 3, 4, 5};
			flex_array<int, 5> f2{6, 7, 8, 9, 10};

			REQUIRE_EQ(f <=> f2, std::strong_ordering::less);
			REQUIRE_EQ(f <=> f, std::strong_ordering::equal);
			REQUIRE_EQ(f2 <=> f, std::strong_ordering::greater);
		}

		SUBCASE("no-cmp") {
			struct uncomparable {};
			flex_array<uncomparable, 5> f; // checking if this compiles
		}
	}

	TEST_CASE("dynamic size but bounded") {
		static_assert(sizeof(flex_array<int, dynamic_extent, 2>) == 2*sizeof(int) + sizeof(size_t));
		static_assert(alignof(flex_array<int, dynamic_extent, 2>) == alignof(size_t));
		static_assert(flex_array<int, dynamic_extent, 1>::mode == flex_array_mode::direct_dynamic_limited_size);

		SUBCASE("default ctor") {
			flex_array<int, dynamic_extent, 5> f;
			check_contents(f, 0);
			check_all_dynamic(f, 0);
		}

		SUBCASE("init list ctor") {
			flex_array<int, dynamic_extent, 5> f{1, 2, 3};
			check_contents(f, 3);
			check_all_dynamic(f, 3);
		}

		SUBCASE("iter ctor") {
			std::array<int, 3> ref{1, 2, 3};
			flex_array<int, dynamic_extent, 5> f(ref.begin(), ref.end());
			check_contents(f, 3);
			check_all_dynamic(f, 3);
		}

		SUBCASE("ctor exceptions") {
			REQUIRE_THROWS_AS((flex_array<int, dynamic_extent, 1>{1, 2}), std::length_error);

			std::array<int, 2> ref{1, 2};
			REQUIRE_THROWS_AS((flex_array<int, dynamic_extent, 1>(ref.begin(), ref.end())), std::length_error);
		}

		SUBCASE("swap") {
			flex_array<int, dynamic_extent, 5> f{1, 2, 3, 4, 5};
			flex_array<int, dynamic_extent, 5> f2{6, 7, 8};

			swap(f, f2);
			REQUIRE(std::ranges::equal(f, std::array{6, 7, 8}));
			REQUIRE(std::ranges::equal(f2, std::array{1, 2, 3, 4, 5}));
		}

		SUBCASE("cmp") {
			flex_array<int, dynamic_extent, 5> f{1, 2, 3, 4, 5};
			flex_array<int, dynamic_extent, 5> f2{6, 7, 8};
			flex_array<int, dynamic_extent, 5> f3{5, 6, 7, 8, 9};

			REQUIRE_EQ(f <=> f2, std::strong_ordering::less);
			REQUIRE_EQ(f <=> f, std::strong_ordering::equal);
			REQUIRE_EQ(f3 <=> f, std::strong_ordering::greater);
		}

		SUBCASE("no-cmp") {
			struct uncomparable {};
			flex_array<uncomparable, dynamic_extent, 5> f; // checking if this compiles
		}
	}

#if __has_include(<ankerl/svector.h>)
	TEST_CASE("dynamic size not bounded") {
		static_assert(sizeof(flex_array<int, 2, dynamic_extent>) == 2*sizeof(int) + sizeof(size_t));
		static_assert(alignof(flex_array<int, 2, dynamic_extent>) == alignof(size_t));
		static_assert(flex_array<int, 1, dynamic_extent>::mode == flex_array_mode::sbo_dynamic_size);

		using farray = flex_array<int, 4, dynamic_extent>;

		SUBCASE("default ctor") {
			farray f;
			check_contents(f, 0);
			check_all_dynamic(f, 0);
		}

		SUBCASE("init list ctor") {
			farray f{1, 2, 3};
			check_contents(f, 3);
			check_all_dynamic(f, 3);
		}

		SUBCASE("iter ctor") {
			std::array<int, 3> ref{1, 2, 3};
			farray f(ref.begin(), ref.end());
			check_contents(f, 3);
			check_all_dynamic(f, 3);
		}

		SUBCASE("ctor larger than size") {
			farray f{1, 2, 3, 4, 5, 6};
			std::array<int, 6> ref{1, 2, 3, 4, 5, 6};
			REQUIRE(std::ranges::equal(f, ref));
		}

		SUBCASE("swap") {
			farray f{1, 2, 3, 4, 5};
			farray f2{6, 7, 8};

			swap(f, f2);
			REQUIRE(std::ranges::equal(f, std::array{6, 7, 8}));
			REQUIRE(std::ranges::equal(f2, std::array{1, 2, 3, 4, 5}));
		}

		SUBCASE("cmp") {
			farray f{1, 2, 3, 4, 5};
			farray f2{6, 7, 8};
			farray f3{5, 6, 7, 8, 9};

			REQUIRE_EQ(f <=> f2, std::strong_ordering::less);
			REQUIRE_EQ(f <=> f, std::strong_ordering::equal);
			REQUIRE_EQ(f3 <=> f, std::strong_ordering::greater);
		}

		SUBCASE("no-cmp") {
			struct uncomparable {};
			flex_array<uncomparable, 5, dynamic_extent> f; // checking if this compiles
		}
	}

	TEST_CASE("converting ctors") {
		SUBCASE("static -> dynamic") {
			flex_array<int, 5> s{1, 2, 3, 4, 5};
			flex_array<int, dynamic_extent, 6> d{s};
			flex_array<int, 5, dynamic_extent> d2{s};

			REQUIRE_EQ(d.size(), 5);
			REQUIRE_EQ(d.max_size(), 6);
			REQUIRE(std::ranges::equal(s, d));
			REQUIRE(std::ranges::equal(s, d2));

			d = s; // checking if this compiles
			d2 = s;
		}

		SUBCASE("dynamic -> static") {
			flex_array<int, dynamic_extent, 5> d{1, 2, 3};
			flex_array<int, 3, dynamic_extent> d2{1, 2, 3};

			flex_array<int, 3> s{d};
			flex_array<int, 3> s2{d2};

			REQUIRE(std::ranges::equal(s, d));
			REQUIRE(std::ranges::equal(s2, d2));

			REQUIRE_THROWS_AS((flex_array<int, 2>{d}), std::length_error);
			REQUIRE_THROWS_AS((flex_array<int, 4>{d}), std::length_error);
			REQUIRE_THROWS_AS((flex_array<int, 2>{d2}), std::length_error);
			REQUIRE_THROWS_AS((flex_array<int, 4>{d2}), std::length_error);
		}
	}
#endif // __has_include
}
