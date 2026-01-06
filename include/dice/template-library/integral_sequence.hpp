#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_SEQUENCE_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_SEQUENCE_HPP

#include <dice/template-library/type_list.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	/**
	 * Generate an integer_sequence for a range
	 *
	 * @tparam Int integral type
	 * @tparam first starting value (inclusive)
	 * @tparam last ending value (exclusive)
	 *
	 * Direction is automatic:
	 * - first == last: empty sequence
	 * - first < last: ascending [first, last)
	 * - first > last: descending (last, first]
	 */
	template<std::integral Int, Int first, Int last>
	using make_integer_sequence = decltype([] {
		if constexpr (first == last) {
			return std::integer_sequence<Int>{};
		} else if constexpr (first < last) {
			// Ascending [first, last)
			auto const impl = []<Int... xs>(std::integer_sequence<Int, xs...>) {
				return std::integer_sequence<Int, (first + xs)...>{};
			};
			return impl(std::make_integer_sequence<Int, last - first>{});
		} else {
			// Descending (last, first]
			auto const impl = []<Int... xs>(std::integer_sequence<Int, xs...>) {
				return std::integer_sequence<Int, (first - xs)...>{};
			};
			return impl(std::make_integer_sequence<Int, first - last>{});
		}
	}());

	/**
	 * Generate a std::index_sequence, i.e., integer_sequence<std::size_t, ...> for a range
	 */
	template<size_t first, size_t last>
	using make_index_sequence = make_integer_sequence<size_t, first, last>;

	/**
		 * Generate a type_list of std::integral_constant<Int, ix> for each ix in the sequence
		 */
	template<std::integral Int, Int first, Int last>
	using make_integral_constant_list = type_list::integer_sequence_to_type_list_t<make_integer_sequence<Int, first, last>>;


	/**
	 * Utilities for integral_template_tuple and integral_template_variant.
	 * Provides a unified interface for generating integer_sequence, index_sequence, type_list, and more.
	 */
	namespace detail_integral_template_util {
		/**
		 * Generate a type_list by applying a template to each index in the sequence
		 *
		 * @tparam first starting value (inclusive)
		 * @tparam last ending value (exclusive)
		 * @tparam T template that takes an integral value, e.g., T<ix>
		 *
		 * Direction is automatic based on first vs last
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_type_list = type_list::transform_t<
				make_integral_constant_list<decltype(first), first, last>,
				[]<decltype(first) ix>(std::type_identity<std::integral_constant<decltype(first), ix>>) {
					return std::type_identity<T<ix>>{};
				}>;

		/**
		 * Check if an index is valid for a given range
		 * Direction is automatically determined from first vs last
		 */
		template<std::integral auto first, decltype(first) last, decltype(first) ix>
		constexpr bool valid_index_v = [] {
			if constexpr (first < last) {
				// Ascending [first, last)
				return ix >= first && ix < last;
			} else if constexpr (first > last) {
				// Descending (last, first]
				return ix <= first && ix > last;
			} else {
				// Empty sequence
				return false;
			}
		}();

		/**
		 * Convert a runtime index to a compile-time offset within the sequence
		 * Direction is automatically determined from first vs last
		 */
		template<std::integral auto first, decltype(first) last>
		constexpr auto to_offset(decltype(first) ix) {
			if constexpr (first < last) {
				// Ascending
				return ix - first;
			} else {
				// Descending
				return first - ix;
			}
		}
	} // namespace detail_integral_template_util

}// namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_SEQUENCE_HPP
