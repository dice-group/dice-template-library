#include <Dice/template-library/switch_template_functions.hpp>

#include <iostream>

using namespace Dice::template_library;

template<std::size_t N>
struct Fib {
	static constexpr int value = Fib<N - 1>::value + Fib<N - 2>::value;
};

template<>
struct Fib<0> {
	static constexpr int value = 0;
};

template<>
struct Fib<1> {
	static constexpr int value = 1;
};

template<std::size_t N>
static constexpr int Fib_v = Fib<N>::value;


template<std::size_t L, std::size_t M, std::size_t N>
static constexpr std::size_t Prod = L *M *N;


int main() {
	{
		std::cout << "Using compile time fib with runtime parameter:\n";
		std::size_t input = 15;
		auto res = switch_cases<0, 20>(input, [](auto i) { return Fib_v<i>; });
		std::cout << "fib(" << input << ") = " << res << '\n';
	}

	{
		std::cout << "Setting a default to be called, when the input is out of range:\n";
		std::size_t input = 100;
		auto res = switch_cases<0, 20>(
				input,
				[](auto i) { return Fib_v<i>; },
				[]() { return -1; });
		std::cout << "fib(" << input << ") = " << res << '\n';
	}

	{
		std::cout << "Working with multiple parameters:\n";
		std::size_t a = 2;
		std::size_t b = 3;
		std::size_t c = 4;
		auto res = switch_cases<0, 5>(a, [&](auto A) {
			return switch_cases<0, 5>(b, [&](auto B) {
				return switch_cases<0, 5>(c, [&](auto C) { return Prod<A, B, C>; });
			});
		});
		std::cout << a << "*" << b << "*" << c << " = " << res << '\n';
	}
}