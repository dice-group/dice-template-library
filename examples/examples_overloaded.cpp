#include <dice/template-library/overloaded.hpp>

#include <cassert>
#include <string>
#include <variant>

int main() {
	auto visitor = dice::template_library::overloaded{
		[](int i) {
			return "Got an int: " + std::to_string(i);
		},
		[](std::string s) {
			return "Got a string: " + s;
		},
		[]([[maybe_unused]] auto u) {
			return std::string{"Got something else"};
		}
	};

	std::variant<std::string, int, double> v = "Hello World";
	auto r1 = std::visit(visitor, v);
	assert(r1 == "Got a string: Hello World");

	v = 42;
	auto r2 = std::visit(visitor, v);
	assert(r2 == "Got an int: 42");

	v = 5.0;
	auto r3 = std::visit(visitor, v);
	assert(r3 == "Got something else");
}
