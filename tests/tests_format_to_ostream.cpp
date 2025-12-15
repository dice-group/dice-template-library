#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/format_to_ostream.hpp>

struct DefinitelyNotOstreamable {
	int x;
};

template<>
struct std::formatter<DefinitelyNotOstreamable> {
	template<typename Ctx>
	constexpr auto parse(Ctx &ctx) {
		return ctx.begin();
	}

	template<typename Ctx>
	auto format(DefinitelyNotOstreamable const &value, Ctx &ctx) const {
		return std::format_to(ctx.out(), "{}", value.x);
	}
};

TEST_SUITE("after format_to_ostream") {
	TEST_CASE("type with explicit ostream overload") {
		std::ostringstream oss;
		oss << 42;

		CHECK_EQ(oss.str(), "42");
	}

	TEST_CASE("type with no explicit ostream overload") {
		SUBCASE("std::vector") {
			std::ostringstream oss;
			oss << std::vector<int>{1, 2, 3};
			CHECK_EQ(oss.str(), "[1, 2, 3]");
		}

		SUBCASE("custom type") {
			std::ostringstream oss;
			oss << DefinitelyNotOstreamable{42};
			CHECK_EQ(oss.str(), "42");
		}
	}
}
