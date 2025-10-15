#include <dice/template-library/iter_to_range.hpp>
#include <iostream>
#include <ranges>

namespace dtl = dice::template_library;

struct iota_iter {
	using value_type = int;

private:
	int cur_;

public:
	explicit iota_iter(int start) noexcept : cur_{start} {
	}

	[[nodiscard]] std::optional<int> next() {
		return cur_++;
	}
};

using iota = dtl::iter_to_range<iota_iter>;
static_assert(std::ranges::range<iota>);

int main() {
	iota ints{0};
	for (int const val : ints | std::views::take(10)) {
		std::cout << val << '\n';
	}
}
