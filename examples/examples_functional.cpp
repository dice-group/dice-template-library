#include <dice/template-library/functional.hpp>
#include <cassert>

int func(int x, int y) {
	return x + y;
}

int main() {
	constexpr auto bound = dice::template_library::bind_front<func>(1);
	assert(bound(2) == 3);
}
