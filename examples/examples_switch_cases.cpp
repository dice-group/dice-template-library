#include <dice/template-library/switch_cases.hpp>

#include <iostream>

using namespace dice::template_library;

template<std::size_t N>
struct fib {
	static constexpr int value = fib<N - 1>::value + fib<N - 2>::value;
};

template<>
struct fib<0> {
	static constexpr int value = 0;
};

template<>
struct fib<1> {
	static constexpr int value = 1;
};

template<std::size_t N>
static constexpr int fib_v = fib<N>::value;


template<std::size_t L, std::size_t M, std::size_t N>
static constexpr std::size_t Prod = L *M *N;


int main() {
	{
		std::cout << "Using compile time fib with runtime parameter:\n";
		std::size_t input = 15;
		auto res = switch_cases<0, 20>(input, []<auto i>() { return fib_v<i>; });
		std::cout << "fib(" << input << ") = " << res << '\n';
	}

	{
		std::cout << "Setting a default to be called, when the input is out of range:\n";
		std::size_t input = 100;
		auto res = switch_cases<0, 20>(
				input,
				[]<auto i>() { return fib_v<i>; },
				[]() { return -1; });
		std::cout << "fib(" << input << ") = " << res << '\n';
	}

	{
		std::cout << "Working with multiple parameters:\n";
		std::size_t a = 2;
		std::size_t b = 3;
		std::size_t c = 4;
		auto res = switch_cases<0, 5>(a, [&b, &c]<auto A>() {
			return switch_cases<0, 5>(b, [&c]<auto B>() {
				return switch_cases<0, 5>(c, []<auto C>() { return Prod<A, B, C>; });
			});
		});
		std::cout << a << "*" << b << "*" << c << " = " << res << '\n';
	}
}
