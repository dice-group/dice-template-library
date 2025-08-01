#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/pool_allocator.hpp>

#include <array>
#include <cstddef>
#include <type_traits>

TEST_SUITE("pool allocator") {
	TEST_CASE("basic pool functions work") {
		dice::template_library::pool<sizeof(long)> pool;

		auto *ptr = static_cast<int *>(pool.allocate(sizeof(int)));
		*ptr = 123;
		REQUIRE_EQ(*ptr, 123);
		pool.deallocate(ptr, sizeof(int));

		auto *ptr2 = static_cast<int *>(pool.allocate(sizeof(int)));
		REQUIRE_EQ(ptr, ptr2);
		*ptr2 = 456;
		REQUIRE_EQ(*ptr2, 456);
		pool.deallocate(ptr2, sizeof(int));

		auto *ptr3 = static_cast<long *>(pool.allocate(sizeof(long)));
		REQUIRE_EQ(static_cast<void *>(ptr2), static_cast<void *>(ptr3));
		*ptr3 = 678;
		REQUIRE_EQ(*ptr3, 678);
		pool.deallocate(ptr3, sizeof(long));

		auto *ptr4 = static_cast<std::array<long, 2> *>(pool.allocate(sizeof(std::array<long, 2>)));
		(*ptr4)[0] = 123;
		(*ptr4)[1] = 456;
		REQUIRE_EQ((*ptr4)[0], 123);
		REQUIRE_EQ((*ptr4)[1], 456);
		pool.deallocate(ptr4, sizeof(std::array<long, 2>));
	}

	TEST_CASE("many allocations and deallocations") {
		dice::template_library::pool_allocator<std::byte, 8, 16> const alloc;
		dice::template_library::pool_allocator<uint64_t, 8, 16> alloc1 = alloc; // first pool
		dice::template_library::pool_allocator<std::array<uint64_t, 2>, 8, 16> alloc2 = alloc; // second pool
		dice::template_library::pool_allocator<std::array<uint64_t, 4>, 8, 16> alloc3 = alloc; // fallback to new

		for (size_t ix = 0; ix < 1'000'000; ++ix) {
			auto *ptr1 = alloc1.allocate(1);
			auto *ptr2 = alloc2.allocate(1);
			auto *ptr3 = alloc3.allocate(1);

			alloc2.deallocate(ptr2, 1);
			alloc3.deallocate(ptr3, 1);
			alloc1.deallocate(ptr1, 1);
		}
	}

	TEST_CASE("allocator interface") {
		using allocator_type = dice::template_library::pool_allocator<uint64_t, 8, 16>;
		using allocator_traits = std::allocator_traits<allocator_type>;

		static_assert(std::is_same_v<typename allocator_traits::value_type, uint64_t>);
		static_assert(std::is_same_v<typename allocator_traits::pointer, uint64_t *>);
		static_assert(std::is_same_v<typename allocator_traits::const_pointer, uint64_t const *>);
		static_assert(std::is_same_v<typename allocator_traits::void_pointer, void *>);
		static_assert(std::is_same_v<typename allocator_traits::const_void_pointer, void const *>);
		static_assert(std::is_same_v<typename allocator_traits::difference_type, std::ptrdiff_t>);
		static_assert(std::is_same_v<typename allocator_traits::size_type, size_t>);
		static_assert(std::is_same_v<typename allocator_traits::propagate_on_container_copy_assignment, std::true_type>);
		static_assert(std::is_same_v<typename allocator_traits::propagate_on_container_move_assignment, std::true_type>);
		static_assert(std::is_same_v<typename allocator_traits::propagate_on_container_swap, std::true_type>);
		static_assert(std::is_same_v<typename allocator_traits::is_always_equal, std::false_type>);
		static_assert(std::is_same_v<typename allocator_traits::template rebind_alloc<int64_t>, dice::template_library::pool_allocator<int64_t, 8, 16>>);
		static_assert(std::is_same_v<typename allocator_traits::template rebind_traits<int64_t>, std::allocator_traits<dice::template_library::pool_allocator<int64_t, 8, 16>>>);

		allocator_type alloc;

		uint64_t *ptr = allocator_traits::allocate(alloc, 1);
		*ptr = 123;
		REQUIRE_EQ(*ptr, 123);
		allocator_traits::deallocate(alloc, ptr, 1);

		auto cpy = alloc; // copy ctor
		auto mv = std::move(cpy); // move ctor
		cpy = alloc; // copy assignment
		mv = std::move(cpy); // move assignment
		swap(mv, alloc); // swap

		dice::template_library::pool_allocator<int, 8, 16> const alloc2 = alloc; // converting constructor
		allocator_traits::template rebind_alloc<int> const alloc3 = alloc;

		static_assert(std::is_same_v<decltype(alloc2), decltype(alloc3)>);
		REQUIRE_EQ(alloc2, alloc3);

		auto alloc4 = allocator_traits::select_on_container_copy_construction(alloc);
		REQUIRE_EQ(mv, alloc);
		REQUIRE_EQ(alloc, alloc4);

		allocator_type alloc5{alloc.underlying_pool()};
		REQUIRE_EQ(alloc5, alloc);
	}
}
