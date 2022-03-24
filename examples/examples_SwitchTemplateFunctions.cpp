#include <Dice/SwitchTemplateFunctions.hpp>
#include <iostream>

using namespace Dice::templateLibrary;

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
				input, [](auto i) { return Fib_v<i>; }, []() { return -1; });
		std::cout << "fib(" << input << ") = " << res << '\n';
	}
}
