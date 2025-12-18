#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_COMMON_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_COMMON_HPP

#include <dice/template-library/type_list.hpp>


#include <concepts>
#include <cstdint>
#include <utility>

namespace dice::template_library {
	/**
	 * Direction for integral template sequences
	 */
	enum struct direction : int8_t {
		ascending = +1,
		descending = -1
	};

	namespace it_detail_v2 {
		/**
		 * Compile-time constant that determines if a sequence will be empty.
		 */
		template<direction Dir, std::integral auto first, decltype(first) last>
		constexpr bool sequence_empty_v = Dir == direction::ascending ? first >= last : first <= last;

		/**
		 * Variable template to check if an index is in the valid range
		 * - ascending: [first, last)
		 * - descending: (last, first]
		 */
		template<direction Dir, std::integral auto first, decltype(first) last, decltype(first) ix>
		constexpr bool valid_index_v = []() consteval {
			if constexpr (Dir == direction::ascending) {
				static_assert(ix >= first && ix < last, "Index out of range for ascending sequence [first, last)");
			} else {
				static_assert(ix <= first && ix > last, "Index out of range for descending sequence (last, first]");
			}
			return true;
		}();

		/**
		 * generates the std::integer_sequence based on direction
		 * - ascending: std::integer_sequence<Int, first, ..., last-1> (exclusive upper bound)
		 * - descending: std::integer_sequence<Int, first, ..., last+1> (exclusive lower bound)
		 *
		 * @tparam Dir direction of the sequence
		 * @tparam Int the integral type of the std::integer_sequence
		 * @tparam first the starting integer (inclusive)
		 * @tparam last the end integer (exclusive)
		 */
		template<direction Dir, std::integral Int, Int first, Int last, Int... ixs>
		constexpr auto make_integer_sequence(std::integer_sequence<Int, ixs...> = {}) {
			auto static constexpr non_empty = !sequence_empty_v<Dir, first, last>;
			if constexpr (non_empty) {
				std::integer_sequence<Int, ixs..., first> const acc;

				static constexpr auto next = static_cast<Int>(first + static_cast<int64_t>(Dir));
				if constexpr (next == last) {
					return acc;
				} else {
					return make_integer_sequence<Dir, Int, next, last>(acc);
				}
			} else {
				return std::integer_sequence<Int, ixs...>{};
			}
		}

		/**
		 * Helper to convert logical index to physical index (offset in underlying storage)
		 * - ascending: ix - first
		 * - descending: first - ix
		 */
		template<direction Dir, std::integral auto first, decltype(first) last>
		constexpr auto make_index(decltype(first) ix) {
			if constexpr (Dir == direction::ascending) {
				return ix - first;
			} else {
				return first - ix;
			}
		}

		/**
		 * Generates a type_list<T<ix>...> based on direction
		 * from a type_list<std::integral_constant<ix>...>
		 * which is generated from a std::integer_sequence<first, ..., last-1>
		 */
		template<direction Dir, std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_type_list = type_list::transform_t<
				type_list::integer_sequence_to_type_list_t<decltype(make_integer_sequence<Dir, decltype(first), first, last>())>,
				[]<decltype(first) ix>(std::type_identity<std::integral_constant<decltype(first), ix>>) {
					return std::type_identity<T<ix>>{};
				}>;
	}// namespace it_detail_v2

}// namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_COMMON_HPP
