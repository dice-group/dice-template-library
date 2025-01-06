#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/pool_allocator.hpp>

TEST_SUITE("pool allocator") {
	TEST_CASE("sanity check") {
		dice::template_library::pool<8, 16> pool;



	}
}
