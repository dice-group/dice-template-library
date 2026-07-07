#include <cassert>
#include <iostream>
#include <string>

import dice.template_library;

int main() {
	assert(sizeof(dice::template_library::static_string) < sizeof(std::string));

	dice::template_library::static_string str{"Hello World"};
	std::cout << str << std::endl;
}
