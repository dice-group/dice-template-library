#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/macro_util.hpp>

#define MY_IDENT world

TEST_SUITE("macro_util") {
	TEST_CASE("concat idents") {
		int DICE_IDENT_CONCAT(hello_, world) = 42;
		CHECK_EQ(hello_world, 42);

		DICE_IDENT_CONCAT(hello_, MY_IDENT) = 12;
		CHECK_EQ(hello_world, 12);
	}
}