#include <dice/template-library/integral_template_tuple.hpp>

#include <array>
#include <iostream>

using namespace dice::template_library;

template<std::size_t N>
struct int_array : std::array<int, N> {
private:
	template<std::size_t... IDs>
	int_array(int value, std::index_sequence<IDs...>) : std::array<int, N>{((void) IDs, value)...} {}

public:
	int_array() = default;
	int_array(int value) : int_array(value, std::make_index_sequence<N>{}) {}
};

template<std::size_t N>
std::ostream &operator<<(std::ostream &os, int_array<N> const &arr) {
	if (N == 0) { return os << "0: []"; }
	os << N << ": [";
	for (std::size_t i = 0; i < N - 1; ++i) { os << arr[i] << ", "; }
	return os << arr[N - 1] << ']';
}

int main() {
	{
		std::cout << "tuple of integer arrays, size 5 to 8, default constructor:\n";
		integral_template_tuple<int_array, 5, 8> itt;
		std::cout << "  " << itt.get<5>() << '\n';
		std::cout << "  " << itt.get<6>() << '\n';
		std::cout << "  " << itt.get<7>() << '\n';
		std::cout << "  " << itt.get<8>() << "\n\n";
	}

	{
		std::cout << "tuple of integer arrays, size 5 to 8, specific constructor:\n";
		auto itt = make_integral_template_tuple<int_array, 5, 8>(42);
		std::cout << "  " << itt.get<5>() << '\n';
		std::cout << "  " << itt.get<6>() << '\n';
		std::cout << "  " << itt.get<7>() << '\n';
		std::cout << "  " << itt.get<8>() << "\n\n";
	}

	{
		std::cout << "tuple of integer arrays, size 5 to 8, cast down:\n";
		integral_template_tuple<int_array, 5, 8> itt;
		auto *casted_itt = reinterpret_cast<integral_template_tuple<int_array, 5, 6> *>(&itt);
		std::cout << "  " << casted_itt->get<5>() << '\n';
		std::cout << "  " << casted_itt->get<6>() << '\n';
	}
}
