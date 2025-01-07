#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/pool_allocator.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

TEST_SUITE("pool allocator") {
	TEST_CASE("sanity check") {
		dice::template_library::pool<8, 16> pool;
		auto alloc1 = pool.get_allocator<uint64_t>(); // first pool
		auto alloc2 = pool.get_allocator<std::array<uint64_t, 2>>(); // second pool
		auto alloc3 = pool.get_allocator<std::array<uint64_t, 4>>(); // fallback to new

		for (size_t ix = 0; ix < 1'000'000; ++ix) {
			auto *ptr1 = alloc1.allocate(1);
			auto *ptr2 = alloc2.allocate(1);
			auto *ptr3 = alloc3.allocate(1);

			alloc2.deallocate(ptr2, 1);
			alloc3.deallocate(ptr3, 1);
			alloc1.deallocate(ptr1, 1);
		}
	}
}
