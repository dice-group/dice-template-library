#include <cassert>
#include <format>
#include <ranges>
#include <string>
#include <vector>

#include <dice/template-library/fmt_join.hpp>

namespace dtl = dice::template_library;
using namespace std::literals;

int main() {
	std::vector ints = {10, 20, 30};

	// Basic usage with string and char separators
	assert(std::format("{}", dtl::fmt_join(ints, ", ")) == "10, 20, 30");
	assert(std::format("{}", dtl::fmt_join({1, 2, 3}, '-')) == "1-2-3");
	assert(std::format("{}", dtl::fmt_join(std::vector{'c', 'c'}, "")) == "cc");

	// Composability with views and forwarding format specifiers
	auto floats = ints | std::views::transform([](int i) { return static_cast<float>(i); });
	assert(std::format("{:.2f}",
					   dtl::fmt_join(floats | std::views::drop(1), "; ")) ==
		   "20.00; 30.00");
}