#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_TUPLE_V2_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_TUPLE_V2_HPP

#include <dice/template-library/integral_sequence.hpp>
#include <dice/template-library/standard_layout_tuple.hpp>
#include <dice/template-library/tuple_algorithm.hpp>
#include <dice/template-library/type_list.hpp>
#include <dice/template-library/type_traits.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	namespace itt_detail_v2 {
		/**
		 * Generates a standard_layout_tuple<T<ix>...> from a type_list<T<ix>...>
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_tuple = type_list::apply_t<integral_sequence::make_type_list<first, last, T>, standard_layout_tuple>;
	}// namespace itt_detail_v2

	/**
	 * A std::tuple-like type holding elements T<ix> for each ix in the sequence
	 *
	 * Direction is automatic:
	 * - first < last: ascending [first, last) → T<first> .. T<last-1>
	 * - first > last: descending (last, first] → T<first> .. T<last+1>
	 * - first == last: empty tuple
	 *
	 * @tparam first first ix to provide to T<ix> (inclusive)
	 * @tparam last last ix (exclusive, not included in the tuple)
	 * @tparam T the template that gets instantiated with T<ix>
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct integral_template_tuple_v2 : itt_detail_v2::make_tuple<first, last, T> {
		using index_type = decltype(first);

		template<index_type ix>
		using value_type = T<ix>;

	private:
		template<index_type ix>
		static constexpr void check_ix() {
			static_assert(integral_sequence::valid_index_v<first, last, ix>, "Index out of range");
		}

		using base = itt_detail_v2::make_tuple<first, last, T>;

		template<index_type ix>
		static consteval size_t make_index() {
			return static_cast<size_t>(integral_sequence::to_offset<first, last>(ix));
		}

	public:
		using base::base;

		/**
		 * Get the element with the type value_type<ix>
		 *
		 * @tparam ix index in the valid range (depends on direction)
		 * @return reference to the element of type value_type<ix>
		 */
		template<index_type ix, typename Self>
		[[nodiscard]] constexpr decltype(auto) get(this Self &&self) noexcept {
			check_ix<ix>();
			return dice::template_library::forward_like<Self>(self.base::template get<make_index<ix>()>());
		}

		/**
		 * Visits each element using visitor
		 *
		 * @param visitor function to be called on each element
		 * @return whatever the last invocation (on the last element) of visitor returned
		 * @note the order of operations is not guaranteed
		 *
		 * @example
		 * A typical use case for the return value would be accumulation of some value over all elements:
		 *
		 * @code
		 * integral_template_tuple_v2<1, 5, some_container_type> tup; // contains T<1>, T<2>, T<3>, T<4>
		 *
		 * auto combined_size = tup.visit([acc = 0ul](auto const &c) mutable {
		 *     return acc += c.size();
		 * });
		 * @endcode
		 */
		template<typename Self, typename F>
		constexpr decltype(auto) visit(this Self &&self, F &&visitor) {
			using base_ref_t = decltype(dice::template_library::forward_like<Self>(std::declval<base &>()));
			return dice::template_library::tuple_for_each(static_cast<base_ref_t>(self), std::forward<F>(visitor));
		}

		/**
		 * Returns a reference to the subtuple of this obtained by dropping every element T<IX> where IX not in the new range
		 *
		 * @tparam new_first new first value (inclusive)
		 * @tparam new_last new last value (exclusive)
		 * @return subtuple
		 */
		template<index_type new_first, index_type new_last, typename Self>
		constexpr decltype(auto) subtuple(this Self &&self) noexcept {
			static_assert(std::is_standard_layout_v<integral_template_tuple_v2>,
						  "Types inhabiting the integral_template_tuple_impl must be standard layout for subtuple to work correctly.");

			auto static constexpr sub_last = [] {
				if constexpr (first < last) {
					// Ascending
					static_assert(new_first >= first && new_last <= last, "Indices for subtuple must be in range.");
					return new_last - new_first;
				} else {
					// Descending
					static_assert(new_first <= first && new_last >= last, "Indices for subtuple must be in range.");
					return new_first - new_last;
				}
			}();
			using new_tuple = integral_template_tuple_v2<new_first, new_last, T>;
			return reinterpret_cast<copy_cvref_t<decltype(std::forward<Self>(self)), new_tuple>>(
					dice::template_library::forward_like<Self>(self.base::template subtuple<make_index<new_first>(), sub_last>()));
		}

		constexpr auto operator<=>(integral_template_tuple_v2 const &other) const noexcept = default;
		constexpr bool operator==(integral_template_tuple_v2 const &other) const noexcept = default;
	};

}// namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_TUPLE_V2_HPP
