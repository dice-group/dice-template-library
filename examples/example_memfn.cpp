#include <algorithm>
#include <dice/template-library/memfn.hpp>

#include <cassert>
#include <ranges>
#include <string>
#include <vector>

struct NumberProcessor {
	std::vector<int> numbers_ = {1, 2, 3, 4, 5, 6, 7, 8};
	std::string prefix_ = "item-";

	static bool is_even(int n) { return n % 2 == 0; }

	[[nodiscard]] std::string decorate(int n) const { return prefix_ + std::to_string(n); }

	[[nodiscard]] std::string item_6() const { return "item-6"; }


	void example() {
		auto processed_view = numbers_ |
							  std::views::filter(DICE_MEMFN(is_even)) |
							  std::views::transform(DICE_MEMFN(decorate));

		assert(std::ranges::equal(processed_view, std::vector<std::string>{"item-2", "item-4", "item-6", "item-8"}));

		assert(std::ranges::any_of(processed_view, [this](auto const &e) { return e == DICE_MEMFN(item_6)();}));
	}
};

int main() {
	NumberProcessor processor;
	processor.example();
	return 0;
}