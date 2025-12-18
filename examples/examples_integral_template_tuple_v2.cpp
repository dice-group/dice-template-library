#include <dice/template-library/integral_template_tuple_v2.hpp>

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
		std::cout << "tuple of integer arrays, size 5 to 8 (exclusive), default constructor:\n";
		integral_template_tuple_v2<5UL, 9, int_array> itt;
		std::cout << "  " << itt.template get<5>() << '\n';
		std::cout << "  " << itt.template get<6>() << '\n';
		std::cout << "  " << itt.template get<7>() << '\n';
		std::cout << "  " << itt.template get<8>() << "\n\n";
	}

	{
		std::cout << "tuple of integer arrays, size 5 to 8 (exclusive), specific constructor:\n";
		integral_template_tuple_v2<5UL, 9, int_array> itt{uniform_construct, 42};
		std::cout << "  " << itt.template get<5>() << '\n';
		std::cout << "  " << itt.template get<6>() << '\n';
		std::cout << "  " << itt.template get<7>() << '\n';
		std::cout << "  " << itt.template get<8>() << "\n\n";
	}

	{
		std::cout << "tuple of integer arrays, size 5 to 8 (exclusive), subtuple:\n";
		integral_template_tuple_v2<5UL, 9, int_array> itt;
		auto &sub_itt = itt.template subtuple<6, 8>();
		std::cout << "  " << sub_itt.template get<6>() << '\n';
		std::cout << "  " << sub_itt.template get<7>() << "\n\n";
	}

	{
		std::cout << "reverse tuple of integer arrays, size 8 down to 5 (exclusive), default constructor:\n";
		integral_template_tuple_rev_v2<8UL, 4, int_array> itt;
		std::cout << "  " << itt.template get<8>() << '\n';
		std::cout << "  " << itt.template get<7>() << '\n';
		std::cout << "  " << itt.template get<6>() << '\n';
		std::cout << "  " << itt.template get<5>() << '\n';
	}
}
