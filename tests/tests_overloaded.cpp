#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/overloaded.hpp>

#include <variant>

TEST_SUITE("overloaded") {
	TEST_CASE("variant") {
		std::variant<int, double, float> v{std::in_place_type<int>, 5};

		std::visit(dice::template_library::overloaded{
			[](int x) {
				CHECK_EQ(x, 5);
			},
			[](double d) {
				FAIL("not expecting double");
			},
			[](auto f) {
				FAIL("not expecting float");
			}
		}, v);

		dice::template_library::match(v,
			[](int x) {
				CHECK_EQ(x, 5);
			},
			[](double d) {
				FAIL("not expecting double");
			},
			[](auto f) {
				FAIL("not expecting float");
			}
		);
	}
}
