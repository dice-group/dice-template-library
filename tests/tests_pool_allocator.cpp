#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/pool_allocator.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

TEST_SUITE("pool allocator") {
	TEST_CASE("basic pool functions work") {
		dice::template_library::pool<sizeof(long)> pool;

		auto *ptr = static_cast<int *>(pool.allocate(sizeof(int)));
		*ptr = 123;
		CHECK_EQ(*ptr, 123);
		pool.deallocate(ptr, sizeof(int));

		auto *ptr2 = static_cast<int *>(pool.allocate(sizeof(int)));
		CHECK_EQ(ptr, ptr2);
		*ptr2 = 456;
		CHECK_EQ(*ptr2, 456);
		pool.deallocate(ptr2, sizeof(int));

		auto *ptr3 = static_cast<long *>(pool.allocate(sizeof(long)));
		CHECK_EQ(static_cast<void *>(ptr2), static_cast<void *>(ptr3));
		*ptr3 = 678;
		CHECK_EQ(*ptr3, 678);
		pool.deallocate(ptr3, sizeof(long));

		auto *ptr4 = static_cast<std::array<long, 2> *>(pool.allocate(sizeof(std::array<long, 2>)));
		(*ptr4)[0] = 123;
		(*ptr4)[1] = 456;
		CHECK_EQ((*ptr4)[0], 123);
		CHECK_EQ((*ptr4)[1], 456);
		pool.deallocate(ptr4, sizeof(std::array<long, 2>));
	}

	TEST_CASE("many allocations and deallocations") {
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

	TEST_CASE("allocator interface") {
		using pool_type = dice::template_library::pool<8, 16>;
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

		pool_type pool;
		allocator_type alloc = pool.get_allocator<uint64_t>();

		uint64_t *ptr = allocator_traits::allocate(alloc, 1);
		*ptr = 123;
		CHECK_EQ(*ptr, 123);
		allocator_traits::deallocate(alloc, ptr, 1);

		auto cpy = alloc; // copy ctor
		auto mv = std::move(cpy); // move ctor
		cpy = alloc; // copy assignment
		mv = std::move(cpy); // move assignment
		swap(mv, alloc); // swap
		dice::template_library::pool_allocator<int, 8, 16> const alloc2 = alloc; // converting constructor
		auto alloc3 = allocator_traits::select_on_container_copy_construction(alloc);

		CHECK_EQ(mv, alloc);
		CHECK_EQ(alloc, alloc3);
	}
}
