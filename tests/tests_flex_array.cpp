#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/flex_array.hpp>
#include <algorithm>
#include <numeric>

namespace dice::template_library {
	// extern template sometimes make problems if internal types are too eagerly instantiated
	extern template struct flex_array<int, 5>;
	extern template struct flex_array<int, dynamic_extent, 5>;

	template struct flex_array<int, 5>;
	template struct flex_array<int, dynamic_extent, 5>;
} // namespace dice::template_library

TEST_SUITE("flex_array") {
	using namespace dice::template_library;

	template<size_t extent, size_t max_extent>
	void check_all_static(flex_array<int, extent, max_extent> const &f) {
		CHECK_FALSE(f.empty());
		CHECK_EQ(f.size(), 5);
		CHECK_EQ(f.max_size(), 5);
		CHECK_EQ(f.capacity(), 5);
		CHECK_EQ(std::distance(f.begin(), f.end()), 5);
		CHECK_EQ(std::distance(f.cbegin(), f.cend()), 5);
		CHECK_EQ(std::distance(f.rbegin(), f.rend()), 5);
		CHECK_EQ(std::distance(f.crbegin(), f.crend()), 5);
	}

	template<size_t extent, size_t max_extent>
	void check_contents(flex_array<int, extent, max_extent> const &f, size_t expected_size) {
		std::vector<int> ref;
		ref.resize(expected_size);
		std::iota(ref.begin(), ref.end(), 1);

		CHECK_EQ(std::accumulate(f.begin(), f.end(), 0), std::accumulate(ref.begin(), ref.end(), 0));

		if (expected_size > 0) {
			CHECK_EQ(*f.data(), 1);
		}

		CHECK_EQ(*(f.data() + f.size() - 1), expected_size);
		CHECK(std::ranges::equal(ref, std::span{f}));
	}

	void check_all_dynamic(flex_array<int, dynamic_extent, 5> &f, size_t expected_size) {
		CHECK_EQ(f.empty(), expected_size == 0);
		CHECK_EQ(f.size(), expected_size);
		CHECK_EQ(f.max_size(), 5);
		CHECK_EQ(f.capacity(), 5);
		CHECK_EQ(std::distance(f.begin(), f.end()), expected_size);
		CHECK_EQ(std::distance(f.cbegin(), f.cend()), expected_size);
		CHECK_EQ(std::distance(f.rbegin(), f.rend()), expected_size);
		CHECK_EQ(std::distance(f.crbegin(), f.crend()), expected_size);

		f.resize(5);
		CHECK_FALSE(f.empty());
		CHECK_EQ(f.size(), 5);
		CHECK_EQ(f.max_size(), 5);
		CHECK_EQ(f.capacity(), 5);
		CHECK_EQ(std::distance(f.begin(), f.end()), 5);
		CHECK_EQ(std::distance(f.cbegin(), f.cend()), 5);
		CHECK_EQ(std::distance(f.rbegin(), f.rend()), 5);
		CHECK_EQ(std::distance(f.crbegin(), f.crend()), 5);

		f[0] = 1;
		f[1] = 2;
		f[2] = 3;
		f[3] = 4;
		f[4] = 5;
		CHECK_EQ(std::accumulate(f.begin(), f.end(), 0), 15);
		CHECK_EQ(*f.data(), 1);
		CHECK_EQ(*(f.data() + f.size() - 1), 5);
	}

	TEST_CASE("static size") {
		static_assert(sizeof(flex_array<int, 1>) == sizeof(int));
		static_assert(alignof(flex_array<int, 1>) == alignof(int));

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
			CHECK(std::ranges::equal(f, std::array{6, 7, 8, 9, 10}));
			CHECK(std::ranges::equal(f2, std::array{1, 2, 3, 4, 5}));
		}

		SUBCASE("cmp") {
			flex_array<int, 5> f{1, 2, 3, 4, 5};
			flex_array<int, 5> f2{6, 7, 8, 9, 10};

			CHECK_EQ(f <=> f2, std::strong_ordering::less);
			CHECK_EQ(f <=> f, std::strong_ordering::equal);
			CHECK_EQ(f2 <=> f, std::strong_ordering::greater);
		}

		SUBCASE("no-cmp") {
			struct uncomparable {};
			flex_array<uncomparable, 5> f; // checking if this compiles
		}
	}

	TEST_CASE("dynamic size") {
		static_assert(sizeof(flex_array<int, dynamic_extent, 2>) == 2*sizeof(int) + sizeof(size_t));
		static_assert(alignof(flex_array<int, dynamic_extent, 2>) == alignof(size_t));

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
			CHECK_THROWS_AS((flex_array<int, dynamic_extent, 1>{1, 2}), std::length_error);

			std::array<int, 2> ref{1, 2};
			CHECK_THROWS_AS((flex_array<int, dynamic_extent, 1>(ref.begin(), ref.end())), std::length_error);
		}

		SUBCASE("swap") {
			flex_array<int, dynamic_extent, 5> f{1, 2, 3, 4, 5};
			flex_array<int, dynamic_extent, 5> f2{6, 7, 8};

			swap(f, f2);
			CHECK(std::ranges::equal(f, std::array{6, 7, 8}));
			CHECK(std::ranges::equal(f2, std::array{1, 2, 3, 4, 5}));
		}

		SUBCASE("cmp") {
			flex_array<int, dynamic_extent, 5> f{1, 2, 3, 4, 5};
			flex_array<int, dynamic_extent, 5> f2{6, 7, 8};
			flex_array<int, dynamic_extent, 5> f3{5, 6, 7, 8, 9};

			CHECK_EQ(f <=> f2, std::strong_ordering::less);
			CHECK_EQ(f <=> f, std::strong_ordering::equal);
			CHECK_EQ(f3 <=> f, std::strong_ordering::greater);
		}

		SUBCASE("no-cmp") {
			struct uncomparable {};
			flex_array<uncomparable, dynamic_extent, 5> f; // checking if this compiles
		}
	}

	TEST_CASE("converting ctors") {
		SUBCASE("static -> dynamic") {
			flex_array<int, 5> s{1, 2, 3, 4, 5};
			flex_array<int, dynamic_extent, 6> d{s};

			CHECK_EQ(d.size(), 5);
			CHECK_EQ(d.max_size(), 6);
			CHECK(std::ranges::equal(s, d));
		}

		SUBCASE("dynamic -> static") {
			flex_array<int, dynamic_extent, 5> d{1, 2, 3};
			flex_array<int, 3> s{d};

			CHECK(std::ranges::equal(s, d));

			CHECK_THROWS_AS((flex_array<int, 2>{d}), std::length_error);
			CHECK_THROWS_AS((flex_array<int, 4>{d}), std::length_error);
		}
	}
}
