#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/polymorphic_allocator.hpp>

#include <memory>
#include <memory_resource>

TEST_SUITE("polymorphic_allocator") {
	template<typename T>
	using poly_alloc_t = dice::template_library::polymorphic_allocator<T, std::allocator, std::pmr::polymorphic_allocator>;

	TEST_CASE("offset_ptr_stl_allocator") {
		using alloc_t = dice::template_library::offset_ptr_stl_allocator<int>;

		alloc_t alloc;
		auto ptr = std::allocator_traits<alloc_t>::allocate(alloc, 1);
		*ptr = 5;
		CHECK(*ptr == 5);
		std::allocator_traits<alloc_t>::deallocate(alloc, ptr, 1);
	}

	TEST_CASE("polymorphic alloc") {
		SUBCASE("with default alloc") {
			poly_alloc_t<int> alloc{};
			CHECK(alloc.template holds_allocator<std::allocator>());


			auto ptr = std::allocator_traits<poly_alloc_t<int>>::allocate(alloc, 1);
			*ptr = 123;
			CHECK(*ptr == 123);
			std::allocator_traits<poly_alloc_t<int>>::deallocate(alloc, ptr, 1);
		}

		SUBCASE("in place construction") {
			SUBCASE("in place index") {
				poly_alloc_t<int> alloc{std::in_place_index<1>, std::pmr::get_default_resource()};

				auto ptr = std::allocator_traits<poly_alloc_t<int>>::allocate(alloc, 1);
				*ptr = 456;
				CHECK(*ptr == 456);
				std::allocator_traits<poly_alloc_t<int>>::deallocate(alloc, ptr, 1);
			}

			SUBCASE("in place type") {
				poly_alloc_t<int> alloc{std::in_place_type<std::pmr::polymorphic_allocator<int>>, std::pmr::get_default_resource()};

				auto ptr = std::allocator_traits<poly_alloc_t<int>>::allocate(alloc, 1);
				*ptr = 456;
				CHECK(*ptr == 456);
				std::allocator_traits<poly_alloc_t<int>>::deallocate(alloc, ptr, 1);
			}
		}

		SUBCASE("type rebind") {
			poly_alloc_t<int> alloc{};
			poly_alloc_t<double> alloc2{alloc};
		}

		SUBCASE("implicit conversion") {
			poly_alloc_t<int> alloc{std::allocator<int>{}};
			CHECK(alloc.template holds_allocator<std::allocator>());

			poly_alloc_t<int> alloc2{std::pmr::polymorphic_allocator<int>{}};
			CHECK(alloc2.template holds_allocator<std::pmr::polymorphic_allocator>());
		}

		SUBCASE("select on container copy construction") {
			poly_alloc_t<int> alloc{};
			auto alloc2 = alloc.select_on_container_copy_construction();
		}
	}
}
