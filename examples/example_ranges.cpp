#include <dice/template-library/ranges.hpp>

#include <algorithm>
#include <cassert>
#include <vector>

namespace dtl = dice::template_library;

int main() {
	std::vector const vec{2, 2, 4, 6, 4};
	auto even = [](int val) { return val % 2 == 0; };

	// Use the range adaptors directly
	assert((vec | dtl::all_of(even)));
	assert((vec | dtl::any_of(even)));
	assert(!(vec | dtl::none_of(even)));

	assert(!(vec | dtl::empty));
	assert(vec | dtl::non_empty);

	assert(!(vec | dtl::all_distinct()));

	auto uniq = vec | dtl::unique;
	assert(std::ranges::equal(uniq, std::vector{2, 4, 6}));

	assert(std::ranges::equal(dtl::range(1, 4, 1), std::vector{1, 2, 3}));
}
