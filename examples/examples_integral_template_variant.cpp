#include <dice/template-library/integral_template_variant.hpp>

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
		std::cout << "get:\n";
		integral_template_variant<5UL, 9, int_array> itv{int_array<6>{1}};
		std::cout << "  " << itv.template get<6>() << '\n';

		itv = integral_template_variant<5UL, 9, int_array>{int_array<7>{2}};
		std::cout << "  " << itv.template get<7>() << "\n\n";
	}

	{
		std::cout << "visit:\n";
		integral_template_variant<5UL, 9, int_array> itv{int_array<6>{3}};

		itv.visit([](auto const &array) {
			std::cout << array << "\n\n";
		});
	}

	{
		std::cout << "in-place construction:\n";
		integral_template_variant<1UL, 10, int_array> itv{std::in_place_type<int_array<7>>, 78};
		std::cout << itv.template get<7>() << '\n';
	}
}
