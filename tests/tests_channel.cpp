#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/channel.hpp>

#include <iterator>
#include <ranges>

TEST_SUITE("mpmc_channel") {
	using namespace dice::template_library;

	TEST_CASE("is range") {
		static_assert(std::input_iterator<channel<int>::iterator>);
		static_assert(std::ranges::range<channel<int>>);
		static_assert(std::ranges::input_range<channel<int>>);
	}

	// TODO tests??
}
