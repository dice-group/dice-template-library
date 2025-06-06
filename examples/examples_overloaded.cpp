#include <dice/template-library/overloaded.hpp>
#include <dice/template-library/variant2.hpp>

#include <cassert>
#include <format>
#include <string>
#include <variant>

namespace dtl = dice::template_library;


int main() {
	auto visitor = dtl::overloaded{
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

	// Using match for single-use visitors
	auto r4 = dtl::match(
			v,
			[](int x) { return x; },
			[](double d) { return static_cast<int>(d); },
			[]([[maybe_unused]]auto f) { return 0; });
	assert(r4 == 5);

	// also works with variant2
	dtl::variant2<int, double> v2 =42.3;
	auto r5 = dtl::visit(visitor, v2);
	assert(r5 == "Got something else");

	auto r6 = dice::template_library::match(
			v2,
			[](int x) { return x; },
			[](double d) { return static_cast<int>(d); },
			[]([[maybe_unused]]auto f) { return 0; });
	assert(r6 == 42);
	return 0;
}
