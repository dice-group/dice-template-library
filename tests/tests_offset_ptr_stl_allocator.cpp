#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/offset_ptr_stl_allocator.hpp>

TEST_SUITE("offset_ptr_stl_allocator") {
	TEST_CASE("sanity check") {
		using alloc_t = dice::template_library::offset_ptr_stl_allocator<int>;

		alloc_t alloc;
		auto ptr = std::allocator_traits<alloc_t>::allocate(alloc, 1);
		*ptr = 5;
		CHECK(*ptr == 5);
		std::allocator_traits<alloc_t>::deallocate(alloc, ptr, 1);
	}
}
