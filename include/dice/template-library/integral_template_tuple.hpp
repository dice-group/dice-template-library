#ifndef HYPERTRIE_INTEGRALTEMPLATEDTUPLE_HPP
#define HYPERTRIE_INTEGRALTEMPLATEDTUPLE_HPP

#include <dice/template-library/standard_layout_tuple.hpp>
#include <dice/template-library/tuple_algorithm.hpp>
#include <dice/template-library/type_list.hpp>
#include <dice/template-library/type_traits.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	namespace itt_detail {
		/**
		 * generates the std::integer_sequence<Int, first, ..., last>
		 * @note first is allowed to be greater than last
		 *
		 * @tparam Int the integral type of the std::integer_sequence
		 * @tparam first the starting integer
		 * @tparam last the end integer
		 */
		template<std::integral Int, Int first, Int last, Int ...ixs>
		constexpr auto make_integer_sequence(std::integer_sequence<Int, ixs...> = {}) {
			std::integer_sequence<Int, ixs..., first> const acc;

			if constexpr (first == last) {
				return acc;
			} else if constexpr (first < last) {
				return make_integer_sequence<Int, first + 1, last>(acc);
			} else {
				return make_integer_sequence<Int, first - 1, last>(acc);
			}
		}

		/**
		 * Generates a type_list<T<first>, ..., T<last>>
		 * from a type_list<std::integral_constant<first>, ..., std::integral_constant<last>>
		 * which is generated from a std::integer_sequence<first, ..., last>
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_type_list = type_list::transform_t<
			type_list::integer_sequence_to_type_list_t<decltype(make_integer_sequence<decltype(first), first, last>())>,
			[]<decltype(first) ix>(std::type_identity<std::integral_constant<decltype(first), ix>>) {
				return std::type_identity<T<ix>>{};
			}
		>;

		/**
		 * Generates a standard_layout_tuple<T<first>, ..., T<last>>
		 * from a type_list<T<first>, ..., T<last>>
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_tuple = type_list::apply_t<make_type_list<first, last, T>, standard_layout_tuple>;

	} // namespace itt_detail

	/**
	 * A std::tuple-like type holding elements T<first> .. T<last> (inclusive).
	 *
	 * @tparam first first ix to provide to T<ix>
	 * @tparam last last ix to provide to T<ix>
	 * @tparam T the template that gets instantiated with T<ix> for ix in first..last (inclusive)
	 *
	 * @note first is allowed to be smaller then last.
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct integral_template_tuple : itt_detail::make_tuple<first, last, T> {
		using index_type = decltype(first);

		template<index_type ix>
		using value_type = T<ix>;

	private:
		using base = itt_detail::make_tuple<first, last, T>;

		template<index_type ix>
		static consteval void check_ix() {
			static_assert(ix >= std::min(first, last) && ix <= std::max(first, last), "Index out of range");
		}

		template<index_type ix>
		static consteval size_t make_index() {
			return first <= last ? static_cast<size_t>(ix - first) : static_cast<size_t>(first - ix);
		}

	public:
		using base::base;

		/**
		 * Get the element with the type value_type<ix>
		 *
		 * @tparam ix index in range [first, ..., last]
		 * @return reference to the element of type value_type<ix>
		 */
		template<index_type ix, typename Self>
		[[nodiscard]] constexpr decltype(auto) get(this Self &&self) noexcept {
			check_ix<ix>();
			return std::forward<Self>(self).base::template get<make_index<ix>()>();
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
		 * integral_template_tuple<1, 5, some_container_type> tup;
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
		 * Returns a reference to the subtuple of this obtained by dropping every element T<IX> where IX not in [new_first, ..., new_last] (inclusive)
		 *
		 * @tparam new_first new first value
		 * @tparam new_last new last value
		 * @return subtuple
		 */
		template<index_type new_first, index_type new_last, typename Self>
		constexpr decltype(auto) subtuple(this Self &&self) noexcept {
			static_assert(std::is_standard_layout_v<integral_template_tuple>,
						  "Types inhabiting the integral_template_tuple must be standard layout for subtuple to work correctly.");
			static_assert(first <= last ? (new_first >= first && new_last <= last) : (new_first <= first && new_last >= last),
						  "Indices for subtuple must be in range.");

			using new_tuple = integral_template_tuple<new_first, new_last, T>;

			// SAFETY: integral_template_tuple has the exact same layout as standard_layout_tuple (and is standard layout)
			return reinterpret_cast<copy_cvref_t<decltype(std::forward<Self>(self)), new_tuple>>(
				std::forward<Self>(self).base::template subtuple<make_index<new_first>(), make_index<new_last>() + 1>());
		}

		constexpr auto operator<=>(integral_template_tuple const &other) const noexcept = default;
		constexpr bool operator==(integral_template_tuple const &other) const noexcept = default;
	};
}// namespace dice::template_library

#endif//HYPERTRIE_INTEGRALTEMPLATEDTUPLE_HPP
