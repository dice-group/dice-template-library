#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/limit_allocator.hpp>

TEST_SUITE("limit_allocator sanity check") {
	using namespace dice::template_library;

	template<limit_allocator_syncness syncness>
	void run_tests() {
		limit_allocator<int> alloc{12 * sizeof(int)};

		auto a = alloc.allocate(5);
		auto b = alloc.allocate(5);
		auto c = alloc.allocate(1);
		alloc.deallocate(c, 1);
		alloc.deallocate(b, 5);

		REQUIRE_THROWS(alloc.allocate(8));

		auto d = alloc.allocate(7);

		alloc.deallocate(a, 5);
		alloc.deallocate(d, 7);

		auto e = alloc.allocate(12);
		alloc.deallocate(e, 12);

		REQUIRE_THROWS(alloc.allocate(13));
	}

	TEST_CASE("sync") {
		run_tests<limit_allocator_syncness::sync>();
	}

	TEST_CASE("unsync") {
		run_tests<limit_allocator_syncness::unsync>();
	}
}
