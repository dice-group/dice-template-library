#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/dbg.hpp>

struct move_only {
	int val;

	explicit move_only(int val) noexcept : val{val} {
	}

	move_only(move_only const &other) = delete;
	move_only &operator=(move_only const &other) = delete;
	move_only(move_only &&other) noexcept = default;
	move_only &operator=(move_only &&other) noexcept = default;
	~move_only() noexcept = default;
};

template<>
struct std::formatter<move_only> {
	template<typename Ctx>
	constexpr auto parse(Ctx &ctx) {
		return ctx.begin();
	}

	template<typename Ctx>
	auto format(move_only const &obj, Ctx &ctx) const {
		return std::format_to(ctx.out(), "{}", obj.val);
	}
};


TEST_SUITE("DICE_DBG") {
	TEST_CASE("basic") {
		int const my_int = 42;

		DICE_DBG(my_int);
		DICE_DBG(5 + 5);
	}

	TEST_CASE("forwarding") {
		int const cint = 42;
		static_assert(std::is_same_v<decltype(DICE_DBG(cint)), int const &>);
		static_assert(std::is_same_v<decltype(DICE_DBG(std::move(cint))), int const &&>);

		int mint = 42;
		static_assert(std::is_same_v<decltype(DICE_DBG(mint)), int &>);
		static_assert(std::is_same_v<decltype(DICE_DBG(std::move(mint))), int &&>);

		static_assert(std::is_same_v<decltype(DICE_DBG(42)), int &&>);
	}

	TEST_CASE("move only type") {
		move_only movo{123};
		auto movo2 = DICE_DBG(std::move(movo));
		movo2 = DICE_DBG(move_only{99});
	}
}
