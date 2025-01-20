#include <dice/template-library/limit_allocator.hpp>

#include <cassert>
#include <vector>


int main() {
	std::vector<int, dice::template_library::limit_allocator<int>> vec{dice::template_library::limit_allocator<int>{3 * sizeof(int)}};
	vec.push_back(1);
	vec.push_back(2);

	try {
		vec.push_back(3);
		assert(false);
	} catch (...) {
	}

	vec.pop_back();
	vec.push_back(4);

	try {
		vec.push_back(5);
		assert(false);
	} catch (...) {
	}
}
