#include <Dice/template-library/integral_templated_tuple.hpp>

#include <array>
#include <iostream>

using namespace Dice::template_library;

template<std::size_t N>
struct intArray : std::array<int, N> {
private:
	template<std::size_t... IDs>
	intArray(int value, std::index_sequence<IDs...>) : std::array<int, N>{((void) IDs, value)...} {}

public:
	intArray() = default;
	intArray(int value) : intArray(value, std::make_index_sequence<N>{}) {}
};

template<std::size_t N>
std::ostream &operator<<(std::ostream &os, intArray<N> const &arr) {
	if (N == 0) { return os << "0: []"; }
	os << N << ": [";
	for (std::size_t i = 0; i < N - 1; ++i) { os << arr[i] << ", "; }
	return os << arr[N - 1] << ']';
}

int main() {
	{
		std::cout << "tuple of integer arrays, size 5 to 8, default constructor:\n";
		IntegralTemplatedTuple<intArray, 5, 8> itt;
		std::cout << "  " << itt.get<5>() << '\n';
		std::cout << "  " << itt.get<6>() << '\n';
		std::cout << "  " << itt.get<7>() << '\n';
		std::cout << "  " << itt.get<8>() << "\n\n";
	}

	{
		std::cout << "tuple of integer arrays, size 5 to 8, specific constructor:\n";
		auto itt = make_integral_template_tuple<intArray, 5, 8>(42);
		std::cout << "  " << itt.get<5>() << '\n';
		std::cout << "  " << itt.get<6>() << '\n';
		std::cout << "  " << itt.get<7>() << '\n';
		std::cout << "  " << itt.get<8>() << "\n\n";
	}

	{
		std::cout << "tuple of integer arrays, size 5 to 8, cast down:\n";
		IntegralTemplatedTuple<intArray, 5, 8> itt;
		auto *casted_itt = reinterpret_cast<IntegralTemplatedTuple<intArray, 5, 6> *>(&itt);
		std::cout << "  " << casted_itt->get<5>() << '\n';
		std::cout << "  " << casted_itt->get<6>() << '\n';
	}
}
