#include <dice/template-library/switch_cases.hpp>

int main() {
	int i = 7;
	auto res = dice::template_library::switch_cases<5, 20>(
			i, [](auto i) -> int { return i * 2; },
			[]() -> int { return -1; });
	if (res != 14)
		exit(1);
}