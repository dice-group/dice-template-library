#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/fmt_join.hpp>

#include <doctest/doctest.h>

#include <format>
#include <list>
#include <ranges>
#include <string>
#include <vector>

namespace dtl = dice::template_library;

TEST_SUITE("join formatter") {

	TEST_CASE("basic joining with vectors and lists") {
		std::vector<int> numbers = {1, 2, 3, 4, 5};
		CHECK(std::format("{}", dtl::fmt_join(numbers, ", ")) == "1, 2, 3, 4, 5");

		std::list<std::string> words = {"one", "two", "three"};
		CHECK(std::format("{}", dtl::fmt_join(words, " | ")) == "one | two | three");

		// Test with a different separator type (char)
		CHECK(std::format("{}", dtl::fmt_join(numbers, '-')) == "1-2-3-4-5");
	}

	TEST_CASE("edge cases: empty and single-element ranges") {
		std::vector<int> empty_vec;
		CHECK(std::format("{}", dtl::fmt_join(empty_vec, ", ")) == "");

		std::vector<std::string> single_word_vec{"hello"};
		// No separator should be printed for a single element.
		CHECK(std::format("{}", dtl::fmt_join(single_word_vec, ", ")) == "hello");
	}

	TEST_CASE("joining temporary ranges") {
		auto result = std::format("{}", dtl::fmt_join(std::vector{10, 20, 30}, "->"));
		CHECK(result == "10->20->30");
	}

	TEST_CASE("compose with range views") {
		std::vector<double> prices = {1.25, 3.79, 10.99, 42.49};

		// Transform doubles to integers (cents)
		auto to_cents = [](double price) {
			return static_cast<int>(price * 100);
		};
		auto cents_view = prices | std::views::transform(to_cents);
		CHECK(std::format("{}", dtl::fmt_join(cents_view, " ")) == "125 379 1099 4249");

		// Chain multiple views
		std::vector<int> data = {5, 4, 3, 2, 1};
		auto processed_view = data | std::views::reverse | std::views::take(3);// becomes {1, 2, 3}
		CHECK(std::format("{}", dtl::fmt_join(processed_view, ", ")) == "1, 2, 3");
	}

	TEST_CASE("Forwarding format specifiers to elements") {
		std::vector<double> values = {1.2345, 2.3456, 3.4567};
		auto formatted = std::format("{:.2f}", dtl::fmt_join(values, "; "));
		CHECK(formatted == "1.23; 2.35; 3.46");

		std::vector<int> hex_values = {10, 15, 255};
		auto hex_formatted = std::format("{:#x}", dtl::fmt_join(hex_values, " "));
		CHECK(hex_formatted == "0xa 0xf 0xff");
	}
}