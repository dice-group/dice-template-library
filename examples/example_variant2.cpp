#include <cassert>
#include <string>
#include <variant>

#include <dice/template-library/overloaded.hpp>
#include <dice/template-library/variant2.hpp>

namespace dtl = dice::template_library;
using namespace std::literals;

int main() {
	// example adapted from from https://en.cppreference.com/w/cpp/utility/variant

	dtl::variant2<int, float> v;
	dtl::variant2<int, float> w;

	v = 42;// v contains int
	int i = dtl::get<int>(v);
	assert(42 == i);// succeeds
	w = dtl::get<int>(v);
	w = dtl::get<0>(v);// same effect as the previous line
	w = v;             // same effect as the previous line

	//  dtl::get<double>(v); // error: no double in [int, float]
	//  dtl::get<3>(v);      // error: valid index values are 0 and 1

	try {
		(void) dtl::get<float>(w);// w contains int, not float: will throw
	} catch (dtl::bad_variant_access const &ex) {
		assert(ex.what() == "bad variant access"s);
	}

	dtl::variant<std::string> x("abc");
	// converting constructors work when unambiguous
	x = "def";// converting assignment also works when unambiguous

	dtl::variant<std::string, void const *> y("abc");
	// casts to void const* when passed a char const*
	assert(dtl::holds_alternative<void const *>(y));// succeeds
	y = "xyz"s;
	assert(dtl::holds_alternative<std::string>(y));// succeeds

	auto visitor = dtl::overloaded{
			[](int x) { return x; },
			[](double d) { return static_cast<int>(d); }};

	auto z = dtl::visit(visitor, v);
	assert(z == 42);
}
