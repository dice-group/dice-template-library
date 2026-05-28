#include <format>
#include <iostream>
#include <sstream>
#include <string>

// Import the overload that makes it ostreamable.
import dice.template_library;

// A type that has no std::ostream operator<< defined
struct not_ostreamable {
	int value;
};

template<>
struct std::formatter<not_ostreamable> {
	template<typename Ctx>
	constexpr auto parse(Ctx &ctx) {
		return ctx.begin();
	}

	template<typename Ctx>
	auto format(not_ostreamable const &nos, Ctx &ctx) const {
		// careful to pass `nos.value` instead of `nos` to the formatter to avoid infinite recursions.
		return std::format_to(ctx.out(), "{}", nos.value);
	}
};


int main() {
	not_ostreamable e{1};
	std::cout << e;
}

