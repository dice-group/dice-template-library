#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/ranges.hpp>

#include <array>
#include <cmath>
#include <iterator>
#include <list>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace dtl = dice::template_library;

TEST_SUITE("range adaptors for all_of, any_of, none_of") {

	constexpr std::array a = {2, 4, 6};
	constexpr auto even = [](int x) { return x % 2 == 0; };
	static_assert(std::ranges::all_of(a, even));
	static_assert(dtl::all_of(even)(a));
	static_assert(!dtl::none_of(even)(a));
	static_assert(dtl::any_of(even)(a));

	auto is_even = [](int x) { return x % 2 == 0; };

	TEST_CASE("std::vector") {
		std::vector v = {2, 4, 6};
		CHECK((v | dtl::all_of(is_even)));
		CHECK(!(v | dtl::none_of(is_even)));
		CHECK((v | dtl::any_of(is_even)));
	}

	TEST_CASE("std::list") {
		std::list l = {1, 2, 3};
		CHECK(!(l | dtl::all_of(is_even)));
		CHECK(!(l | dtl::none_of(is_even)));
		CHECK((l | dtl::any_of(is_even)));
	}

	TEST_CASE("std::array") {
		std::array a = {0, 0, 0};
		CHECK((a | dtl::all_of([](int x) { return x == 0; })));
		CHECK(!(a | dtl::none_of([](int x) { return x == 0; })));
		CHECK((a | dtl::any_of([](int x) { return x == 0; })));
	}

	TEST_CASE("views pipeline") {
		std::vector v = {1, 2, 3, 4};
		auto view = v | std::views::filter(is_even);
		CHECK((view | dtl::all_of(is_even)));
		CHECK(!(view | dtl::none_of(is_even)));
		CHECK((view | dtl::any_of(is_even)));
	}

	TEST_CASE("temporary ranges") {
		CHECK((std::vector{2, 4, 6} | dtl::all_of(is_even)));
		CHECK(!(std::vector{2, 4, 6} | dtl::none_of(is_even)));
		CHECK((std::vector{2, 4, 6} | dtl::any_of(is_even)));
	}

	TEST_CASE("subrange") {
		int raw[] = {2, 4, 6};
		auto sub = std::ranges::subrange(std::begin(raw), std::end(raw));
		CHECK((sub | dtl::all_of(is_even)));
		CHECK(!(sub | dtl::none_of(is_even)));
		CHECK((sub | dtl::any_of(is_even)));
	}

	TEST_CASE("const range") {
		const std::vector v = {2, 4, 6};
		CHECK((v | dtl::all_of(is_even)));
		CHECK(!(v | dtl::none_of(is_even)));
		CHECK((v | dtl::any_of(is_even)));
	}

	TEST_CASE("stateful lambda capture") {
		int counter = 0;
		auto counted_even = [&counter](int x) {
			++counter;
			return x % 2 == 0;
		};
		std::vector v = {2, 4, 6};
		CHECK((v | dtl::all_of(counted_even)));
		CHECK(counter == static_cast<int>(v.size()));
	}

	TEST_CASE("input iterator single-pass range") {
		std::istringstream input("2 4 6");
		std::istream_iterator<int> begin(input), end;
		auto range = std::ranges::subrange(begin, end);
		CHECK((range | dtl::all_of(is_even)));

		std::istringstream input2("2 4 6");
		auto range2 = std::ranges::subrange(std::istream_iterator<int>(input2), std::istream_iterator<int>());
		CHECK((range2 | dtl::any_of(is_even)));
	}

	TEST_CASE("views join") {
		std::vector<std::vector<int>> nested = {{2, 4}, {6}};
		auto joined = nested | std::views::join;
		CHECK((joined | dtl::all_of(is_even)));
		CHECK((joined | dtl::any_of(is_even)));
		CHECK(!(joined | dtl::none_of(is_even)));
	}
}

TEST_SUITE("empty and non_empty algorithms") {
	TEST_CASE("standard containers that support std::ranges::empty") {
		std::vector<int> const empty_vec{};
		CHECK((empty_vec | dtl::empty));
		CHECK_FALSE((empty_vec | dtl::non_empty));

		std::vector<int> const non_empty_vec{1, 2, 3};
		CHECK_FALSE((non_empty_vec | dtl::empty));
		CHECK((non_empty_vec | dtl::non_empty));
	}

	TEST_CASE("fallback to iterator comparison for input-only ranges") {
		// none-empty input range
		std::istringstream input_with_data("1 2 3");
		auto non_empty_range = std::ranges::subrange(std::istream_iterator<int>(input_with_data),
													 std::istream_iterator<int>());
		CHECK_FALSE((non_empty_range | dtl::empty));
		CHECK((non_empty_range | dtl::non_empty));


		// empty input range
		std::istringstream empty_input("");
		auto empty_range = std::ranges::subrange(std::istream_iterator<int>(empty_input),
												 std::istream_iterator<int>());
		CHECK((empty_range | dtl::empty));
		CHECK_FALSE((empty_range | dtl::non_empty));
	}
}

TEST_SUITE("remove_element adaptor") {
	TEST_CASE("default equality predicate") {
		std::vector<int> input{1, 2, 3, 2, 4};
		auto result = input | dtl::remove_element(2);
		std::vector<int> expected{1, 3, 4};
		CHECK(std::ranges::equal(result, expected));
	}

	TEST_CASE("custom predicate") {
		auto near = [](int a, int b) { return std::abs(a - b) < 2; };
		std::vector<int> input{1, 2, 3, 4, 5};
		auto result = input | dtl::remove_element(3, near);
		std::vector<int> expected{1, 5};
		CHECK(std::ranges::equal(result, expected));
	}
}

TEST_SUITE("all_equal algorithm") {
	TEST_CASE("default equality predicate") {
		CHECK((std::vector<int>{7, 7, 7, 7} | dtl::all_equal()));
		CHECK((std::list<std::string>{"str", "str", "str"} | dtl::all_equal()));
		CHECK((std::vector<int>{42} | dtl::all_equal()));
		CHECK((std::vector<int>{} | dtl::all_equal()));

		CHECK_FALSE((std::vector<int>{7, 7, 1, 7} | dtl::all_equal()));
		CHECK_FALSE((std::list<std::string>{"a", "b", "a"} | dtl::all_equal()));
	}

	TEST_CASE("custom predicate") {
		auto are_near = [](int a, int b) { return std::abs(a - b) <= 2; };

		std::vector<int> all_near_values{10, 12, 11, 9, 11};
		CHECK((all_near_values | dtl::all_equal(are_near)));

		std::vector<int> not_all_near_values{10, 12, 13, 9};
		CHECK_FALSE((not_all_near_values | dtl::all_equal(are_near)));
	}
}

// Helper types for testing the backend selction (set, unordered_set) below

struct ComparableOnly {
	int id;
	auto operator<=>(const ComparableOnly &) const = default;
};

struct HashableOnly {
	int id;
	bool operator==(const HashableOnly &other) const { return id == other.id; }
};

namespace std {
	template<>
	struct hash<HashableOnly> {
		std::size_t operator()(const HashableOnly &h) const noexcept {
			return std::hash<int>{}(h.id);
		}
	};
}// namespace std

TEST_SUITE("unique range adaptor") {

	TEST_CASE("sanity check") {
		std::vector input{1, 2, 1, 3, 4, 2, 1, 5, 4};
		auto result_view = input | dtl::unique;
		std::vector expected{1, 2, 3, 4, 5};
		CHECK(std::ranges::equal(result_view, expected));
	}

	TEST_CASE("edge cases") {
		std::vector<int> empty_vec{};
		auto result_empty_vec = empty_vec | dtl::unique;
		CHECK(std::begin(result_empty_vec) == std::end(result_empty_vec));

		std::vector all_unique{1, 2, 3, 4, 5};
		auto result_all_unique = all_unique | dtl::unique;
		CHECK(std::ranges::equal(result_all_unique, all_unique));

		std::vector<int> all_same{7, 7, 7, 7, 7};
		auto result_all_same = all_same | dtl::unique;
		std::vector expected_single{7};
		CHECK(std::ranges::equal(result_all_same, expected_single));
	}

	TEST_CASE("non-trivial type") {
		std::list<std::string> words{"hello", "world", "hello", "again", "world"};
		auto result_view = words | dtl::unique;
		std::list<std::string> expected{"hello", "world", "again"};
		CHECK(std::ranges::equal(result_view, expected));
	}

	TEST_CASE("fallback to std::set") {
		std::vector<ComparableOnly> const input{{10}, {20}, {10}, {30}};
		auto result_view = input | dtl::unique;
		std::vector<ComparableOnly> expected{{10}, {20}, {30}};
		CHECK(std::ranges::equal(result_view, expected, [](const auto &a, const auto &b) { return a.id == b.id; }));
	}

	TEST_CASE("std::unordered_set for hashable-only types") {
		std::vector<HashableOnly> input{{10}, {20}, {10}, {30}};
		auto result_view = input | dtl::unique;
		std::vector<HashableOnly> expected{{10}, {20}, {30}};
		CHECK(std::ranges::equal(result_view, expected));
	}

	TEST_CASE("single-pass input ranges") {
		std::istringstream input_stream("1 2 1 3 4 2 1 5 4");
		// istream_view is an input_range, it can only be iterated once.
		auto input_range = std::ranges::istream_view<int>(input_stream);

		auto result_view = input_range | dtl::unique;
		std::vector<int> expected{1, 2, 3, 4, 5};

		// Must collect the result into a container to test, as the view is single-pass.
		std::vector<int> result_vec;
		std::ranges::copy(result_view, std::back_inserter(result_vec));

		CHECK(std::ranges::equal(result_vec, expected));
	}
}

TEST_SUITE("range view") {

	TEST_CASE("range(stop) overload") {
		auto view1 = dtl::range<int>(5);
		std::vector expected1{0, 1, 2, 3, 4};
		CHECK(std::ranges::equal(view1, expected1));

		auto view2 = dtl::range<size_t>(3);
		std::vector<size_t> expected2{0, 1, 2};
		CHECK(std::ranges::equal(view2, expected2));

		// A range up to 0 should be empty
		auto view3 = dtl::range<int>(0);
		CHECK(std::ranges::begin(view3) == std::ranges::end(view3));

		// A range up to 1 should contain only 0
		auto view4 = dtl::range<int>(1);
		std::vector expected4{0};
		CHECK(std::ranges::equal(view4, expected4));
	}

	TEST_CASE("range(start, stop) overload") {
		auto view1 = dtl::range<int>(2, 6);
		std::vector expected1{2, 3, 4, 5};
		CHECK(std::ranges::equal(view1, expected1));

		// Start is after stop, should be empty
		auto view2 = dtl::range<int>(5, 2);
		CHECK(std::begin(view2) == std::end(view2));

		// Start is equal to stop, should be empty
		auto view3 = dtl::range<int>(5, 5);
		CHECK(std::begin(view3) == std::end(view3));

		// negative numbers
		auto view4 = dtl::range<int>(-2, 2);
		std::vector expected4{-2, -1, 0, 1};
		CHECK(std::ranges::equal(view4, expected4));
	}

	TEST_CASE("range(start, stop, step) overload") {
		SUBCASE("positive step") {
			auto view = dtl::range<int>(0, 10, 2);
			std::vector<int> expected{0, 2, 4, 6, 8};
			CHECK(std::ranges::equal(view, expected));

			// step overshoots the stop value
			auto view_overshoot = dtl::range<int>(0, 9, 2);
			CHECK(std::ranges::equal(view_overshoot, expected));

			// step is larger than the entire range
			auto view_large_step = dtl::range<int>(0, 10, 20);
			std::vector<int> expected_large{0};
			CHECK(std::ranges::equal(view_large_step, expected_large));

			// wrong direction
			auto view_wrong_dir = dtl::range<int>(10, 0, 2);
			CHECK(std::begin(view_wrong_dir) == std::end(view_wrong_dir));
		}

		SUBCASE("negative step") {
			auto view = dtl::range<int>(10, 0, -2);
			std::vector<int> expected{10, 8, 6, 4, 2};
			CHECK(std::ranges::equal(view, expected));

			auto view_odd = dtl::range<int>(9, 0, -2);
			std::vector<int> expected_odd{9, 7, 5, 3, 1};
			CHECK(std::ranges::equal(view_odd, expected_odd));

			// wrong direction
			auto view_wrong_dir = dtl::range<int>(0, 10, -2);
			CHECK(std::begin(view_wrong_dir) == std::end(view_wrong_dir));
		}
	}
}

TEST_SUITE("all_distinct algorithm") {

	TEST_CASE("standard hashable types, default predicate") {
		CHECK((std::vector<int>{1, 2, 3, 4, 5} | dtl::all_distinct()));
		CHECK((std::list<std::string>{"a", "b", "c"} | dtl::all_distinct()));
		CHECK((std::vector<int>{42} | dtl::all_distinct()));
		CHECK((std::vector<int>{} | dtl::all_distinct()));

		CHECK_FALSE((std::vector<int>{1, 2, 3, 2, 1} | dtl::all_distinct()));
		CHECK_FALSE((std::list<std::string>{"a", "b", "a"} | dtl::all_distinct()));
	}

	TEST_CASE("ComparableOnly type") {
		std::vector<ComparableOnly> const distinct_vec{{1}, {2}, {3}};
		CHECK((distinct_vec | dtl::all_distinct()));

		std::vector<ComparableOnly> const duplicate_vec{{1}, {2}, {1}};
		CHECK_FALSE((duplicate_vec | dtl::all_distinct()));
	}

	TEST_CASE("HashableOnly type") {
		std::vector<HashableOnly> const distinct_vec{{10}, {20}, {30}};
		CHECK((distinct_vec | dtl::all_distinct()));

		std::vector<HashableOnly> const duplicate_vec{{10}, {20}, {10}};
		CHECK_FALSE((duplicate_vec | dtl::all_distinct()));
	}

	TEST_CASE("custom predicate with HashableOnly type") {
		auto same_id_custom = [](const HashableOnly &a, const HashableOnly &b) {
			return a.id == b.id;
		};

		std::vector<HashableOnly> const distinct_items{{10}, {20}, {30}};
		CHECK((distinct_items | dtl::all_distinct(same_id_custom)));

		std::vector<HashableOnly> const duplicate_items{{10}, {20}, {10}};
		CHECK_FALSE((duplicate_items | dtl::all_distinct(same_id_custom)));
	}
}
