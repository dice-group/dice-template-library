#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_TUPLE_V2_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_TUPLE_V2_HPP

#include <dice/template-library/integral_template_common.hpp>
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
		using namespace integral_template_detail;

		/**
		 * Generates a type_list<T<first>, ..., T<last-1>> (ascending, exclusive upper bound)
		 * from a type_list<std::integral_constant<first>, ..., std::integral_constant<last-1>>
		 * which is generated from a std::integer_sequence<first, ..., last-1>
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_type_list_asc = type_list::transform_t<
				type_list::integer_sequence_to_type_list_t<decltype(make_integer_sequence_asc<decltype(first), first, last>())>,
				[]<decltype(first) ix>(std::type_identity<std::integral_constant<decltype(first), ix>>) {
					return std::type_identity<T<ix>>{};
				}>;

		/**
		 * Generates a type_list<T<first>, ..., T<last+1>> (descending, exclusive lower bound)
		 * from a type_list<std::integral_constant<first>, ..., std::integral_constant<last+1>>
		 * which is generated from a std::integer_sequence<first, ..., last+1>
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_type_list_desc = type_list::transform_t<
				type_list::integer_sequence_to_type_list_t<decltype(make_integer_sequence_desc<decltype(first), first, last>())>,
				[]<decltype(first) ix>(std::type_identity<std::integral_constant<decltype(first), ix>>) {
					return std::type_identity<T<ix>>{};
				}>;

		/**
		 * Generates a standard_layout_tuple<T<first>, ..., T<last-1>> (ascending)
		 * from a type_list<T<first>, ..., T<last-1>>
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_tuple_asc = type_list::apply_t<make_type_list_asc<first, last, T>, standard_layout_tuple>;

		/**
		 * Generates a standard_layout_tuple<T<first>, ..., T<last+1>> (descending)
		 * from a type_list<T<first>, ..., T<last+1>>
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_tuple_desc = type_list::apply_t<make_type_list_desc<first, last, T>, standard_layout_tuple>;

	}// namespace itt_detail_v2

	/**
	 * A std::tuple-like type holding elements T<first> .. T<last-1> (exclusive upper bound).
	 *
	 * @tparam first first ix to provide to T<ix> (inclusive)
	 * @tparam last last ix (exclusive, not included in the tuple)
	 * @tparam T the template that gets instantiated with T<ix> for ix in [first, last)
	 *
	 * @note first must be less than last.
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct integral_template_tuple_v2 : itt_detail_v2::make_tuple_asc<first, last, T> {
		static_assert(first < last, "integral_template_tuple_v2 requires first < last. Use integral_template_tuple_rev_v2 for descending ranges.");

		using index_type = decltype(first);

		template<index_type ix>
		using value_type = T<ix>;

	private:
		using base = itt_detail_v2::make_tuple_asc<first, last, T>;

		template<index_type ix>
		static consteval void check_ix() {
			static_assert(ix >= first && ix < last, "Index out of range");
		}

		template<index_type ix>
		static consteval size_t make_index() {
			return static_cast<size_t>(ix - first);
		}

	public:
		using base::base;

		/**
		 * Get the element with the type value_type<ix>
		 *
		 * @tparam ix index in range [first, last)
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
		 * Returns a reference to the subtuple of this obtained by dropping every element T<IX> where IX not in [new_first, new_last)
		 *
		 * @tparam new_first new first value (inclusive)
		 * @tparam new_last new last value (exclusive)
		 * @return subtuple
		 */
		template<index_type new_first, index_type new_last, typename Self>
		constexpr decltype(auto) subtuple(this Self &&self) noexcept {
			static_assert(std::is_standard_layout_v<integral_template_tuple_v2>,
						  "Types inhabiting the integral_template_tuple_v2 must be standard layout for subtuple to work correctly.");
			static_assert(new_first >= first && new_last <= last,
						  "Indices for subtuple must be in range.");

			using new_tuple = integral_template_tuple_v2<new_first, new_last, T>;

			// SAFETY: integral_template_tuple_v2 has the exact same layout as standard_layout_tuple (and is standard layout)
			return reinterpret_cast<copy_cvref_t<decltype(std::forward<Self>(self)), new_tuple>>(
					std::forward<Self>(self).base::template subtuple<make_index<new_first>(), new_last - new_first>());
		}

		constexpr auto operator<=>(integral_template_tuple_v2 const &other) const noexcept = default;
		constexpr bool operator==(integral_template_tuple_v2 const &other) const noexcept = default;
	};

	/**
	 * A std::tuple-like type holding elements T<first> .. T<last+1> (exclusive lower bound, counting down).
	 *
	 * @tparam first first ix to provide to T<ix> (inclusive)
	 * @tparam last last ix (exclusive, not included in the tuple)
	 * @tparam T the template that gets instantiated with T<ix> for ix in (last, first]
	 *
	 * @note first must be greater than last.
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct integral_template_tuple_rev_v2 : itt_detail_v2::make_tuple_desc<first, last, T> {
		static_assert(first > last, "integral_template_tuple_rev_v2 requires first > last. Use integral_template_tuple_v2 for ascending ranges.");

		using index_type = decltype(first);

		template<index_type ix>
		using value_type = T<ix>;

	private:
		using base = itt_detail_v2::make_tuple_desc<first, last, T>;

		template<index_type ix>
		static consteval void check_ix() {
			static_assert(ix <= first && ix > last, "Index out of range");
		}

		template<index_type ix>
		static consteval size_t make_index() {
			return static_cast<size_t>(first - ix);
		}

	public:
		using base::base;

		/**
		 * Get the element with the type value_type<ix>
		 *
		 * @tparam ix index in range (last, first]
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
		 * integral_template_tuple_rev_v2<5, 1, some_container_type> tup; // contains T<5>, T<4>, T<3>, T<2>
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
		 * Returns a reference to the subtuple of this obtained by dropping every element T<IX> where IX not in (new_last, new_first]
		 *
		 * @tparam new_first new first value (inclusive)
		 * @tparam new_last new last value (exclusive)
		 * @return subtuple
		 */
		template<index_type new_first, index_type new_last, typename Self>
		constexpr decltype(auto) subtuple(this Self &&self) noexcept {
			static_assert(std::is_standard_layout_v<integral_template_tuple_rev_v2>,
						  "Types inhabiting the integral_template_tuple_rev_v2 must be standard layout for subtuple to work correctly.");
			static_assert(new_first <= first && new_last >= last,
						  "Indices for subtuple must be in range.");

			using new_tuple = integral_template_tuple_rev_v2<new_first, new_last, T>;

			// SAFETY: integral_template_tuple_rev_v2 has the exact same layout as standard_layout_tuple (and is standard layout)
			return reinterpret_cast<copy_cvref_t<decltype(std::forward<Self>(self)), new_tuple>>(
					std::forward<Self>(self).base::template subtuple<make_index<new_first>(), new_first - new_last>());
		}

		constexpr auto operator<=>(integral_template_tuple_rev_v2 const &other) const noexcept = default;
		constexpr bool operator==(integral_template_tuple_rev_v2 const &other) const noexcept = default;
	};
}// namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_TUPLE_V2_HPP
