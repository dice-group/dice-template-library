#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/flex_array.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace dice::template_library {
	// extern templates sometimes make problems if internal types are too eagerly instantiated
	extern template struct flex_array<int, 5>;
	extern template struct flex_array<int, dynamic_extent, 5>;
	extern template struct flex_array<int, 0>;
	extern template struct flex_array<int, dynamic_extent, 0>;

	template struct flex_array<int, 5>;
	template struct flex_array<int, dynamic_extent, 5>;
	template struct flex_array<int, 0>;
	template struct flex_array<int, dynamic_extent, 0>;
} // namespace dice::template_library

namespace {
	using dice::template_library::dynamic_extent;
	using dice::template_library::flex_array;

	// the bytes of the direct_dynamic_limited_size mode: the size, then the elements
	template<typename T, size_t n>
	struct limited_reference {
		size_t size;
		T data[n];
	};

	struct empty_member {};

	// the bytes of the direct_dynamic_limited_size mode with max_extent 0: the size and no element
	struct limited_reference_0 {
		size_t size;
		empty_member data;
	};

	struct int_array_4 {
		int data[4];
	};

	struct alignas(16) over_aligned {
		uint8_t value;
	};

	// has operator< and operator==, but no operator<=>
	struct less_only {
		int value;

		constexpr bool operator==(less_only const &other) const noexcept = default;

		constexpr bool operator<(less_only const &other) const noexcept {
			return value < other.value;
		}
	};

	struct uncomparable {};

	template<typename T, size_t n>
	constexpr bool check_static_size_layout() {
		using F = flex_array<T, n>;
		static_assert(F::mode == dice::template_library::flex_array_mode::direct_static_size);
		static_assert(std::is_standard_layout_v<F> == std::is_standard_layout_v<T>);
		static_assert(std::is_trivially_copyable_v<F> == std::is_trivially_copyable_v<T>);
		static_assert(sizeof(F) == sizeof(T[n]));
		static_assert(alignof(F) == alignof(T[n]));
		return true;
	}

	template<typename T, size_t n>
	constexpr bool check_limited_size_layout() {
		using F = flex_array<T, dynamic_extent, n>;
		static_assert(F::mode == dice::template_library::flex_array_mode::direct_dynamic_limited_size);
		static_assert(std::is_standard_layout_v<F> == std::is_standard_layout_v<T>);
		static_assert(std::is_trivially_copyable_v<F> == std::is_trivially_copyable_v<T>);
		static_assert(sizeof(F) == sizeof(limited_reference<T, n>));
		static_assert(alignof(F) == alignof(limited_reference<T, n>));
		return true;
	}

	static_assert(std::is_standard_layout_v<int> && std::is_trivially_copyable_v<int>);
	static_assert(std::is_standard_layout_v<over_aligned> && std::is_trivially_copyable_v<over_aligned>);
	static_assert(std::is_trivially_copyable_v<std::optional<uint64_t>>);

	static_assert(check_static_size_layout<int, 1>());
	static_assert(check_static_size_layout<int, 5>());
	static_assert(check_static_size_layout<uint8_t, 3>());
	static_assert(check_static_size_layout<over_aligned, 3>());
	static_assert(check_static_size_layout<std::optional<uint64_t>, 3>());

	static_assert(check_limited_size_layout<int, 1>());
	static_assert(check_limited_size_layout<int, 5>());
	static_assert(check_limited_size_layout<uint8_t, 3>());
	static_assert(check_limited_size_layout<over_aligned, 3>());
	static_assert(check_limited_size_layout<std::optional<uint64_t>, 3>());
} // namespace

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
#define EXTS std::integral_constant<size_t, dynamic_extent>, std::integral_constant<size_t, 4>
#else
#define EXTS std::integral_constant<size_t, dynamic_extent>
#endif

    TEST_CASE_TEMPLATE("dynamic size not bounded", Ext, EXTS) {
		static_assert(sizeof(flex_array<int, Ext::value, dynamic_extent>) == sizeof(void *) + 2*sizeof(size_t));
		static_assert(alignof(flex_array<int, Ext::value, dynamic_extent>) == alignof(size_t));
        static_assert(flex_array<int, Ext::value, dynamic_extent>::mode == (Ext::value == dynamic_extent ? flex_array_mode::dynamic_size : flex_array_mode::sbo_dynamic_size));

		using farray = flex_array<int, Ext::value, dynamic_extent>;

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
#if __has_include(<ankerl/svector.h>)
	    SUBCASE("no-cmp") {
		    struct uncomparable {};
		    flex_array<uncomparable, 5, dynamic_extent> f; // checking if this compiles
	    }
#endif

	}

	TEST_CASE("converting ctors") {
		SUBCASE("static -> dynamic") {
			flex_array<int, 5> s{1, 2, 3, 4, 5};
			flex_array<int, dynamic_extent, 6> d{s};

			REQUIRE_EQ(d.size(), 5);
			REQUIRE_EQ(d.max_size(), 6);
			REQUIRE(std::ranges::equal(s, d));

			d = s; // checking if this compiles

#if __has_include(<ankerl/svector.h>)
		    flex_array<int, 5, dynamic_extent> d2{s};
		    REQUIRE(std::ranges::equal(s, d2));
		    d2 = s;
#endif
		}

	    SUBCASE("dynamic -> static") {
		    flex_array<int, dynamic_extent, 5> d{1, 2, 3};
		    flex_array<int, dynamic_extent> d2{1, 2, 3};

		    flex_array<int, 3> s{d};
		    flex_array<int, 3> s2{d2};

		    REQUIRE(std::ranges::equal(s, d));
		    REQUIRE(std::ranges::equal(s2, d2));

		    REQUIRE_THROWS_AS((flex_array<int, 2>{d}), std::length_error);
		    REQUIRE_THROWS_AS((flex_array<int, 4>{d}), std::length_error);
		    REQUIRE_THROWS_AS((flex_array<int, 2>{d2}), std::length_error);
		    REQUIRE_THROWS_AS((flex_array<int, 4>{d2}), std::length_error);
		}

#if __has_include(<ankerl/svector.h>)
		SUBCASE("sbo dynamic -> static") {
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
#endif //__has_include(<ankerl/svector.h>)
	}
	TEST_CASE("bytes of the direct modes") {
		SUBCASE("static size") {
			static constexpr flex_array<int, 4> f{1, 2, 3, 4};
			static constexpr auto bytes = std::bit_cast<int_array_4>(f);
			static_assert(bytes.data[0] == 1 && bytes.data[1] == 2 && bytes.data[2] == 3 && bytes.data[3] == 4);

			auto const back = std::bit_cast<flex_array<int, 4>>(bytes);
			REQUIRE(back == f);
		}

		SUBCASE("dynamic size but bounded") {
			static constexpr flex_array<int, dynamic_extent, 4> f{1, 2, 3, 4};
			static constexpr auto bytes = std::bit_cast<limited_reference<int, 4>>(f);
			static_assert(bytes.size == 4);
			static_assert(bytes.data[0] == 1 && bytes.data[1] == 2 && bytes.data[2] == 3 && bytes.data[3] == 4);

			auto const back = std::bit_cast<flex_array<int, dynamic_extent, 4>>(bytes);
			REQUIRE(back == f);
		}
	}

	TEST_CASE("extent 0") {
		SUBCASE("static size") {
			using F = flex_array<int, 0>;
			static_assert(F::mode == flex_array_mode::direct_static_size);
			static_assert(std::is_standard_layout_v<F>);
			static_assert(std::is_trivially_copyable_v<F>);
			// holds no element, and an object takes at least one byte
			static_assert(sizeof(F) == 1);
			static_assert(alignof(F) == 1);

			static constexpr F f{};
			static_assert(f.size() == 0);
			static_assert(f.empty());
			static_assert(f.data() == nullptr);
			static_assert(f.begin() == f.end());
			static_assert(f.rbegin() == f.rend());
			static_assert(f == f);
			static_assert((f <=> f) == std::strong_ordering::equal);

			std::span<int const, 0> const s{f};
			REQUIRE(s.empty());

			std::array<int, 1> const ref{1};
			REQUIRE_THROWS_AS((F{1}), std::length_error);
			REQUIRE_THROWS_AS((F(ref.begin(), ref.end())), std::length_error);
			F const g(ref.begin(), ref.begin());
			REQUIRE(g.empty());

			flex_array<int, dynamic_extent, 3> const d{f};
			REQUIRE(d.empty());

			[[maybe_unused]] flex_array<uncomparable, 0> u; // checking if this compiles
		}

		SUBCASE("dynamic size but bounded") {
			using F = flex_array<int, dynamic_extent, 0>;
			static_assert(F::mode == flex_array_mode::direct_dynamic_limited_size);
			static_assert(std::is_standard_layout_v<F>);
			static_assert(std::is_trivially_copyable_v<F>);
			static_assert(sizeof(F) == sizeof(limited_reference_0));
			static_assert(alignof(F) == alignof(limited_reference_0));

			F f;
			REQUIRE(f.empty());
			REQUIRE_EQ(f.size(), 0);
			REQUIRE_EQ(f.max_size(), 0);
			REQUIRE(f.data() == nullptr);
			REQUIRE(f.begin() == f.end());
			REQUIRE(f == F{});
			REQUIRE_EQ(f <=> F{}, std::strong_ordering::equal);

			std::span<int const> const s{f};
			REQUIRE(s.empty());

			f.resize(0);
			REQUIRE(f.empty());
			REQUIRE_THROWS_AS(f.resize(1), std::invalid_argument);
			REQUIRE_THROWS_AS((F{1}), std::length_error);

			[[maybe_unused]] flex_array<uncomparable, dynamic_extent, 0> u; // checking if this compiles
		}
	}

	TEST_CASE("comparison category of the static size") {
		static_assert(std::same_as<std::compare_three_way_result_t<flex_array<int, 2>>, std::strong_ordering>);
		static_assert(std::same_as<std::compare_three_way_result_t<flex_array<double, 2>>, std::partial_ordering>);
		static_assert(std::same_as<std::compare_three_way_result_t<flex_array<less_only, 2>>, std::weak_ordering>);
		static_assert(std::same_as<std::compare_three_way_result_t<flex_array<int, 0>>, std::strong_ordering>);
		static_assert(std::same_as<std::compare_three_way_result_t<flex_array<double, 0>>, std::partial_ordering>);
		static_assert(std::same_as<std::compare_three_way_result_t<flex_array<less_only, 0>>, std::weak_ordering>);

		SUBCASE("element type with operator< only") {
			flex_array<less_only, 2> const a{less_only{1}, less_only{2}};
			flex_array<less_only, 2> const b{less_only{1}, less_only{3}};

			REQUIRE_EQ(a <=> b, std::weak_ordering::less);
			REQUIRE_EQ(b <=> a, std::weak_ordering::greater);
			REQUIRE_EQ(a <=> a, std::weak_ordering::equivalent);
			REQUIRE(a < b);
			REQUIRE(a != b);
			REQUIRE(a == a);
		}

		SUBCASE("partial order") {
			flex_array<double, 2> const a{1.0, 2.0};
			flex_array<double, 2> const nan{1.0, std::numeric_limits<double>::quiet_NaN()};

			REQUIRE_EQ(a <=> a, std::partial_ordering::equivalent);
			REQUIRE_EQ(a <=> nan, std::partial_ordering::unordered);
			REQUIRE_FALSE(nan == nan);
		}
	}
}
