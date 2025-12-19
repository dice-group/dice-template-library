#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_SEQUENCE_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_SEQUENCE_HPP

#include <dice/template-library/for.hpp>
#include <dice/template-library/type_list.hpp>

#include <concepts>
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
	constexpr auto make_integer_sequence() {
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
	}

	/**
		 * Generate a std::index_sequence, i.e., integer_sequence<std::size_t, ...> for a range
		 */
	template<std::size_t first, std::size_t last>
	constexpr auto make_index_sequence() {
		return make_integer_sequence<std::size_t, first, last>();
	}

	/**
		 * Generate a type_list of std::integral_constant<Int, ix> for each ix in the sequence
		 */
	template<std::integral Int, Int first, Int last>
	using make_integral_constant_list = type_list::integer_sequence_to_type_list_t<
			decltype(make_integer_sequence<Int, first, last>())>;

	/**
	 * Utilities for integral_template_tuple and integral_template_variant.
	 * Provides a unified interface for generating integer_sequence, index_sequence, type_list, and more.
	 */
	namespace integral_sequence {
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
		 * Apply a type_list generated from a sequence to a template
		 *
		 * Example: make_template<0, 3, MyTemplate, std::tuple>
		 *          generates std::tuple<MyTemplate<0>, MyTemplate<1>, MyTemplate<2>>
		 */
		template<std::integral auto first, decltype(first) last,
				 template<decltype(first)> typename T, template<typename...> typename Wrapper>
		using make_template = type_list::apply_t<make_type_list<first, last, T>, Wrapper>;

		/**
		 * Check if an index is valid for a given range
		 * Direction is automatically determined from first vs last
		 */
		template<std::integral auto first, decltype(first) last, decltype(first) ix>
		constexpr bool valid_index_v = []() consteval {
			if constexpr (first < last) {
				// Ascending [first, last)
				static_assert(ix >= first && ix < last,
							  "Index out of range for ascending sequence [first, last)");
			} else if constexpr (first > last) {
				// Descending (last, first]
				static_assert(ix <= first && ix > last,
							  "Index out of range for descending sequence (last, first]");
			} else {
				// Empty sequence
				static_assert(first != last, "Index invalid for empty sequence");
			}
			return true;
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

	}// namespace integral_sequence

}// namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_SEQUENCE_HPP
