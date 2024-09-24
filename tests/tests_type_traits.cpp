#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/type_traits.hpp>

TEST_SUITE("type_traits") {

	TEST_CASE("is_zst") {
		struct zst {};
		struct non_zst {
			int x;
		};

		static_assert(dice::template_library::is_zst<zst>::value);
		static_assert(dice::template_library::is_zst_v<zst>);

		static_assert(!dice::template_library::is_zst<non_zst>::value);
		static_assert(!dice::template_library::is_zst_v<non_zst>);

		static_assert(!dice::template_library::is_zst<void>::value);
		static_assert(!dice::template_library::is_zst_v<void>);
	}
}
