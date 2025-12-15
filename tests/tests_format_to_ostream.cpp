#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/format_to_ostream.hpp>

struct definitely_not_ostreamable {
	int x;
};

template<>
struct std::formatter<definitely_not_ostreamable> {
	template<typename Ctx>
	constexpr auto parse(Ctx &ctx) {
		return ctx.begin();
	}

	template<typename Ctx>
	auto format(definitely_not_ostreamable const &value, Ctx &ctx) const {
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
		std::ostringstream oss;
		oss << definitely_not_ostreamable{42};
		CHECK_EQ(oss.str(), "42");
	}
}
