// Include the overload that makes it ostreamable.
#include <dice/template-library/format_to_ostream.hpp>

#include <format>
#include <iostream>
#include <sstream>
#include <string>

// A type that has no std::ostream operator<< defined
struct NotOStreamable {
	int value;
};

template<>
struct std::formatter<NotOStreamable> {
	template<typename Ctx>
	constexpr auto parse(Ctx &ctx) {
		return ctx.begin();
	}

	template<typename Ctx>
	auto format(NotOStreamable const &nos, Ctx &ctx) const {
		// careful to pass `nos.value` instead of `nos` to the formatter to avoid infinite recursions.
		return std::format_to(ctx.out(), "{}", nos.value);
	}
};


int main() {
	NotOStreamable e{1};
	std::cout << e;
}

