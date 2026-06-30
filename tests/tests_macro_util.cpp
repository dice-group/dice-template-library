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

    TEST_CASE("filename") {
	    std::string_view const f = DICE_FILENAME;
	    CHECK(f.ends_with("tests_macro_util.cpp"));
	}

    TEST_CASE("ignore leak") {
	    auto *my_obj = new int{42};
	    dice::template_library::ignore_leak(my_obj);
	}

    TEST_CASE("ignore leak, deprecated macro") {
	    auto *my_obj = new int{42};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	    DICE_IGNORE_LEAK(my_obj);
#pragma GCC diagnostic pop
	}
}
