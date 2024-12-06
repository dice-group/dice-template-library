#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/polymorphic_allocator.hpp>

#include <memory>
#include <memory_resource>
#include <random>

template<typename T>
struct mallocator {
	using value_type = T;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::false_type;

	int x = std::random_device{}();

	mallocator() = default;
	mallocator(mallocator const &) = default;
	mallocator(mallocator &&) = default;
	mallocator &operator=(mallocator const &) = default;
	mallocator &operator=(mallocator &&) = default;

	template<typename U>
	mallocator(mallocator<U> const &other) : x{other.x} {
	}

	T *allocate(size_type n) {
		return static_cast<T *>(malloc(sizeof(T) * n));
	}

	void deallocate(T *ptr, size_type) {
		free(ptr);
	}

	bool operator==(mallocator const &other) const noexcept = default;
	bool operator!=(mallocator const &other) const noexcept = default;
};

template<typename T>
struct mallocator2 {
	using value_type = T;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::false_type;

	int x = std::random_device{}();

	mallocator2() = default;
	mallocator2(mallocator2 const &) = default;
	mallocator2(mallocator2 &&) = default;
	mallocator2 &operator=(mallocator2 const &) = default;
	mallocator2 &operator=(mallocator2 &&) = default;

	template<typename U>
	mallocator2(mallocator2<U> const &other) : x{other.x} {
	}

	T *allocate(size_type n) {
		return static_cast<T *>(malloc(sizeof(T) * n));
	}

	void deallocate(T *ptr, size_type) {
		free(ptr);
	}

	bool operator==(mallocator2 const &other) const noexcept = default;
	bool operator!=(mallocator2 const &other) const noexcept = default;
};

TEST_SUITE("polymorphic_allocator") {

#if __has_include(<boost/interprocess/offset_ptr.hpp>)
	TEST_CASE("offset_ptr_stl_allocator") {
		using alloc_t = dice::template_library::offset_ptr_stl_allocator<int>;

		alloc_t alloc;
		auto ptr = std::allocator_traits<alloc_t>::allocate(alloc, 1);
		*ptr = 5;
		CHECK(*ptr == 5);
		std::allocator_traits<alloc_t>::deallocate(alloc, ptr, 1);
	}

	TEST_CASE("offset_ptr_stl_allocator with std::pmr") {
		using alloc_t = dice::template_library::offset_ptr_stl_allocator<int, std::pmr::polymorphic_allocator>;

		std::pmr::monotonic_buffer_resource rc;
		alloc_t alloc{&rc};

		auto ptr = std::allocator_traits<alloc_t>::allocate(alloc, 1);
		*ptr = 5;
		CHECK(*ptr == 5);
		std::allocator_traits<alloc_t>::deallocate(alloc, ptr, 1);
	}
#endif // __has_include

	template<typename T>
	using poly_alloc2_t = dice::template_library::polymorphic_allocator<T, std::allocator, mallocator>;

	template<typename T>
	using poly_alloc3_t = dice::template_library::polymorphic_allocator<T, std::allocator, mallocator, mallocator2>;


	template<template<typename> typename Alloc>
	void run_test() {
		SUBCASE("copy ctor") {
			Alloc<int> a{};

			int *x = a.allocate(1);
			*x = 5;

			Alloc<int> b{a};
			CHECK_EQ(a, b);

			int *y = b.allocate(1);
			*y = 5;
			CHECK_EQ(*x, *y);
			b.deallocate(x, 1);
			b.deallocate(y, 1);
		}

		SUBCASE("move ctor") {
			Alloc<int> a{};

			int *x = a.allocate(1);
			*x = 5;

			Alloc<int> b{std::move(a)};

			int *y = b.allocate(1);
			*y = 5;
			CHECK_EQ(*x, *y);
			b.deallocate(x, 1);
			b.deallocate(y, 1);
		}

		SUBCASE("copy assign") {
			Alloc<int> a{};
			Alloc<int> b{std::in_place_index<1>};

			CHECK_NE(a, b);
			a = b;
			CHECK_EQ(a, b);

			int *x = a.allocate(1);
			b.deallocate(x, 1);
		}

		SUBCASE("move assign") {
			Alloc<int> a{};
			Alloc<int> b{std::in_place_index<1>};

			CHECK_NE(a, b);
			a = std::move(b);
			b = a;
			CHECK_EQ(a, b);

			int *x = a.allocate(1);
			b.deallocate(x, 1);
		}

		SUBCASE("swap") {
			Alloc<int> a{};
			Alloc<int> b{};
			Alloc<int> c{std::in_place_index<1>};

			CHECK_EQ(a, b);
			CHECK_NE(a, c);
			CHECK_NE(b, c);

			swap(a, b);
			CHECK_EQ(a, b);
			CHECK_NE(a, c);
			CHECK_NE(b, c);

			swap(a, c);
			CHECK_EQ(c, b);
			CHECK_NE(a, b);
			CHECK_NE(a, c);

			swap(a, b);
			CHECK_EQ(a, c);
			CHECK_NE(a, b);
			CHECK_NE(b, c);
		}

		SUBCASE("with default alloc") {
			Alloc<int> alloc{};
			CHECK(alloc.template holds_allocator<std::allocator>());


			auto ptr = std::allocator_traits<Alloc<int>>::allocate(alloc, 1);
			*ptr = 123;
			CHECK(*ptr == 123);
			std::allocator_traits<Alloc<int>>::deallocate(alloc, ptr, 1);
		}

		SUBCASE("in place construction") {
			SUBCASE("in place index") {
				Alloc<int> alloc{std::in_place_index<1>};

				auto ptr = std::allocator_traits<Alloc<int>>::allocate(alloc, 1);
				*ptr = 456;
				CHECK(*ptr == 456);
				std::allocator_traits<Alloc<int>>::deallocate(alloc, ptr, 1);
			}

			SUBCASE("in place type") {
				Alloc<int> alloc{std::in_place_type<mallocator<int>>};

				auto ptr = std::allocator_traits<Alloc<int>>::allocate(alloc, 1);
				*ptr = 456;
				CHECK(*ptr == 456);
				std::allocator_traits<Alloc<int>>::deallocate(alloc, ptr, 1);
			}
		}

		SUBCASE("type rebind") {
			Alloc<int> alloc{};
			Alloc<double> alloc2{alloc};
		}

		SUBCASE("implicit conversion") {
			Alloc<int> alloc{std::allocator<int>{}};
			CHECK(alloc.template holds_allocator<std::allocator>());

			Alloc<int> alloc2{mallocator<int>{}};
			CHECK(alloc2.template holds_allocator<mallocator>());
		}

		SUBCASE("select on container copy construction") {
			Alloc<int> alloc{};
			auto alloc2 = alloc.select_on_container_copy_construction();

			CHECK_EQ(alloc, alloc2);
		}
	}


	TEST_CASE("polymorphic alloc") {
		run_test<poly_alloc2_t>();
		run_test<poly_alloc3_t>();
	}
}
