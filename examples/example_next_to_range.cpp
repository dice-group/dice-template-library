#include <dice/template-library/next_to_range.hpp>

#include <cstddef>
#include <iostream>
#include <iterator>
#include <optional>
#include <ranges>

namespace dtl = dice::template_library;

struct iota_iter_impl {
	using value_type = int;

private:
	int cur_;

public:
	explicit iota_iter_impl(int start = 0) noexcept : cur_{start} {
	}

protected:
	[[nodiscard]] std::optional<int> next() {
		return cur_++;
	}
};

// Create just a C++-style iterator
using iota_iter = dtl::next_to_iter<iota_iter_impl>;
static_assert(std::input_iterator<iota_iter>);

// Create a C++-style range
using iota = dtl::next_to_range<iota_iter_impl>;
static_assert(std::ranges::range<iota>);


int main() {
	size_t num_iter = 0;
	for (auto it = iota_iter{}; num_iter < 10; ++it) {
		std::cout << *it << '\n';
		++num_iter;
	}

	for (int const val : iota{5} | std::views::take(10)) {
		std::cout << val << '\n';
	}
}
