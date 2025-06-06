#include <dice/template-library/ranges.hpp>

#include <cassert>
#include <vector>

namespace dtl = dice::template_library;

int main() {
	std::vector v = {2, 4, 6};
	auto even = [](int x) { return x % 2 == 0; };

	// Use the range adaptors directly
	assert((v | dtl::all_of(even)));
	assert((v | dtl::any_of(even)));
	assert(!(v | dtl::none_of(even)));
}
