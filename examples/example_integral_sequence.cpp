#include <dice/template-library/for.hpp>
#include <dice/template-library/integral_sequence.hpp>

#include <array>
#include <iostream>
#include <variant>

using namespace dice::template_library;
using namespace dice::template_library::integral_sequence;

// Example template that takes an integral value
template<int N>
using IntArray = std::array<int, N>;

int main() {
	std::cout << "=== integral_sequence examples ===\n\n";

	// 1. Generate integer_sequence
	std::cout << "1. Generate integer_sequence:\n";
	{
		// Ascending [3, 7)
		auto seq_asc = make_integer_sequence<int, 3, 7>();
		std::cout << "  Ascending [3, 7): ";
		[&]<int... xs>(std::integer_sequence<int, xs...>) {
			((std::cout << xs << " "), ...);
		}(seq_asc);
		std::cout << "\n";

		// Descending (2, 7]
		auto seq_desc = make_integer_sequence<int, 7, 2>();
		std::cout << "  Descending (2, 7]: ";
		[&]<int... xs>(std::integer_sequence<int, xs...>) {
			((std::cout << xs << " "), ...);
		}(seq_desc);
		std::cout << "\n\n";
	}

	// 2. Generate index_sequence
	std::cout << "2. Generate index_sequence:\n";
	{
		auto idx_seq = make_index_sequence<0, 5>();
		std::cout << "  Ascending [0, 5): ";
		[&]<std::size_t... xs>(std::index_sequence<xs...>) {
			((std::cout << xs << " "), ...);
		}(idx_seq);
		std::cout << "\n\n";
	}

	// 3. Generate type_list of integral_constants
	std::cout << "3. Generate type_list of integral_constants:\n";
	{
		using const_list = make_integral_constant_list<int, 10, 13>;
		type_list::for_each<const_list>([](auto type_id) {
			using T = typename decltype(type_id)::type;
			std::cout << "  integral_constant<int, " << T::value << ">\n";
		});
		std::cout << "\n";
	}

	return 0;
}
