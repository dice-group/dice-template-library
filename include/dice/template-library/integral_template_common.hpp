#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_COMMON_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_COMMON_HPP

#include <concepts>
#include <utility>

namespace dice::template_library {

	namespace integral_template_detail {
		/**
		 * generates the std::integer_sequence<Int, first, ..., last-1> (counting up, exclusive upper bound)
		 *
		 * @tparam Int the integral type of the std::integer_sequence
		 * @tparam first the starting integer (inclusive)
		 * @tparam last the end integer (exclusive)
		 */
		template<std::integral Int, Int first, Int last, Int... ixs>
		constexpr auto make_integer_sequence_asc(std::integer_sequence<Int, ixs...> = {}) {
			std::integer_sequence<Int, ixs..., first> const acc;

			if constexpr (first + 1 == last) {
				return acc;
			} else {
				return make_integer_sequence_asc<Int, first + 1, last>(acc);
			}
		}

		/**
		 * generates the std::integer_sequence<Int, first, ..., last+1> (counting down, exclusive lower bound)
		 *
		 * @tparam Int the integral type of the std::integer_sequence
		 * @tparam first the starting integer (inclusive)
		 * @tparam last the end integer (exclusive)
		 */
		template<std::integral Int, Int first, Int last, Int... ixs>
		constexpr auto make_integer_sequence_desc(std::integer_sequence<Int, ixs...> = {}) {
			std::integer_sequence<Int, ixs..., first> const acc;

			if constexpr (first - 1 == last) {
				return acc;
			} else {
				return make_integer_sequence_desc<Int, first - 1, last>(acc);
			}
		}

		/**
		 * Helper to check if an index is in the valid range [first, last) for ascending sequences
		 */
		template<std::integral auto first, decltype(first) last, decltype(first) ix>
		consteval bool check_index_asc() {
			static_assert(ix >= first && ix < last, "Index out of range for ascending sequence [first, last)");
			return true;
		}

		/**
		 * Helper to check if an index is in the valid range (last, first] for descending sequences
		 */
		template<std::integral auto first, decltype(first) last, decltype(first) ix>
		consteval bool check_index_desc() {
			static_assert(ix <= first && ix > last, "Index out of range for descending sequence (last, first]");
			return true;
		}

		/**
		 * Helper to convert runtime index to compile-time index for ascending sequences
		 * Returns the offset from first
		 */
		template<std::integral auto first, decltype(first) last>
		constexpr auto make_index_asc(decltype(first) ix) {
			return ix - first;
		}

		/**
		 * Helper to convert runtime index to compile-time index for descending sequences
		 * Returns the offset from first
		 */
		template<std::integral auto first, decltype(first) last>
		constexpr auto make_index_desc(decltype(first) ix) {
			return first - ix;
		}
	}// namespace integral_template_detail

}// namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_COMMON_HPP
