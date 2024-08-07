#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/variant2.hpp>

TEST_SUITE("variant2") {
	using namespace dice::template_library;

	TEST_CASE("hash") {
		variant2<int, double> const x{std::in_place_type<int>, 5};
		[[maybe_unused]] auto h = std::hash<variant2<int, double>>{}(x); // only checking if this compiles
	}
}
