/**
 * dice::template_library::integral_template_tuple is **DEPRECATED**.
 * It will be removed in next major release.
 *
 * Use dice::template_library::integral_template_tuple_v2 instead.
 */

#include <array>
#include <iostream>

// disabling deprecated warnings because we run CI with -Werror
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <dice/template-library/integral_template_tuple.hpp>


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
		integral_template_tuple<5UL, 8, int_array> itt;
		std::cout << "  " << itt.template get<5>() << '\n';
		std::cout << "  " << itt.template get<6>() << '\n';
		std::cout << "  " << itt.template get<7>() << '\n';
		std::cout << "  " << itt.template get<8>() << "\n\n";
	}

	{
		std::cout << "tuple of integer arrays, size 5 to 8, specific constructor:\n";
		integral_template_tuple<5UL, 8, int_array> itt{uniform_construct, 42};
		std::cout << "  " << itt.template get<5>() << '\n';
		std::cout << "  " << itt.template get<6>() << '\n';
		std::cout << "  " << itt.template get<7>() << '\n';
		std::cout << "  " << itt.template get<8>() << "\n\n";
	}

	{
		std::cout << "tuple of integer arrays, size 5 to 8, subtuple:\n";
		integral_template_tuple<5UL, 8, int_array> itt;
		auto &sub_itt = itt.template subtuple<6, 7>();
		std::cout << "  " << sub_itt.template get<6>() << '\n';
		std::cout << "  " << sub_itt.template get<7>() << '\n';
	}
}
