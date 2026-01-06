#include <dice/template-library/functional.hpp>
#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

int func(int x, int y) {
	return x + y;
}

struct Adder {
	int offset;
	// using a member function
	[[nodiscard]] int add_offset(int value) const {
		return value + offset;
	}
};

int main() {
	namespace dtl = dice::template_library;

	constexpr auto bound = dtl::bind_front<func>(1);
	assert(bound(2) == 3);
	std::vector<int> numbers{1, 2, 3, 4};
	std::vector<int> expected{51, 52, 53, 54};

	// using a free function
	auto result_view_fre = numbers | std::views::transform(dtl::bind_front<func>(50));
	assert(std::ranges::equal(result_view_fre, expected));

	// using a member function
	auto result_view_member = numbers | std::views::transform(dtl::bind_front<&Adder::add_offset>(Adder{50}));
	assert(std::ranges::equal(result_view_member, expected));
}
