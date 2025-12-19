#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/functional.hpp>

#include <type_traits>

double func(int x, double d) {
	return x + d;
}

double nothrow_func(int x, double d) noexcept {
	return x + d;
}

struct myclass {
	int x;

	int func(int y) const {
		return x + y;
	}

	int nothrow_func(int y) const noexcept {
		return x + y;
	}
};

struct only_movable {
	only_movable() noexcept = default;
	only_movable(only_movable const &) = delete;
	only_movable &operator=(only_movable const &) = delete;
	only_movable(only_movable &&) noexcept = default;
	only_movable &operator=(only_movable &&) noexcept = default;
	~only_movable() noexcept = default;
};

std::array<only_movable, 2> move_test_func(only_movable a, only_movable b) noexcept {
	return {std::move(a), std::move(b)};
}

TEST_CASE("bind_front") {
	using namespace dice::template_library;

	SUBCASE("free func") {
		constexpr auto bound_free_func = bind_front<func>(5);
		constexpr auto bound_nothrow_free_func = bind_front<nothrow_func>(5);

		static_assert(!std::is_nothrow_invocable_v<decltype(bound_free_func), double>);
		CHECK_EQ(bound_free_func(1.0), 6.0);

		static_assert(std::is_nothrow_invocable_v<decltype(bound_nothrow_free_func), double>);
		CHECK_EQ(bound_nothrow_free_func(1.0), 6.0);
	}

	SUBCASE("member func") {
		static constexpr myclass obj{42};
		constexpr auto bound_memfn = bind_front<&myclass::func>(&obj);
		constexpr auto bound_nothrow_memfn = bind_front<&myclass::nothrow_func>(&obj);

		static_assert(!std::is_nothrow_invocable_v<decltype(bound_memfn), int>);
		CHECK_EQ(bound_memfn(1), 43);

		static_assert(std::is_nothrow_invocable_v<decltype(bound_nothrow_memfn), int>);
		CHECK_EQ(bound_nothrow_memfn(1), 43);
	}

	SUBCASE("forwarding") {
		auto bound_func = bind_front<move_test_func>(only_movable{});

		static_assert(std::is_nothrow_invocable_v<decltype(bound_func), only_movable &&>);
		std::move(bound_func)(only_movable{});
	}
}
