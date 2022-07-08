#include <dice/template-library/for.hpp>

#include <cstdint>
#include <iostream>

using namespace dice::template_library;

// Types for constexpr for_values must be constexpr constructible.
struct ConstexprType {
	int i = 5;
	constexpr ConstexprType() {}
	friend std::ostream &operator<<(std::ostream &os, const ConstexprType &dummy) {
		os << dummy.i;
		return os;
	}
};


int main() {
	{

		std::cout << "Print the types of several types:";
		for_types<uint8_t, uint16_t, uint32_t, uint64_t>([]<typename T>() {
			std::cout << " type: " << typeid(T).name() << " size: " << sizeof(T) << std::endl;
		});
	}

	{
		std::cout << "Print some values:";
		for_values<-5, size_t(5), ConstexprType{}>([](auto x) {
			std::cout << " " << x;
		});
		std::cout << std::endl;
	}

	{
		std::cout << "Print increasing and decreasing ranges:";
		for_range<0, 9>([](auto x) {
			std::cout << " " << x;
		});
		std::cout << std::endl;

		for_range<5, -5>([](auto x) {
			std::cout << " " << x;
		});
		std::cout << std::endl;
	}
}