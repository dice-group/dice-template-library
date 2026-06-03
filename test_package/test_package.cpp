#include <cstdlib>

import dice.template_library;

int main() {
	int i = 7;
	auto res = dice::template_library::switch_cases<5, 20>(
			i, [](auto i) -> int { return i * 2; },
			[]() -> int { return -1; });
	if (res != 14)
		std::exit(1);
}