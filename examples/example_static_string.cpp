#include <dice/template-library/static_string.hpp>

#include <cassert>
#include <iostream>
#include <string>

int main() {
	assert(sizeof(dice::template_library::static_string) < sizeof(std::string));

	dice::template_library::static_string str{"Hello World"};
	std::cout << str << std::endl;
}
