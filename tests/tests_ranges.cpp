#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/ranges.hpp>

#include <array>
#include <iterator>
#include <list>
#include <ranges>
#include <sstream>
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
