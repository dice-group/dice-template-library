#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATED_VARIANT_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATED_VARIANT_HPP

#include <algorithm>
#include <concepts>
#include <utility>
#include <variant>

namespace dice::template_library {

	namespace itv_detail {

		/**
		 * generates the std::integer_sequence<Int, FIRST, ..., LAST>
		 * @note FIRST is allowed to be greater than LAST
		 *
		 * @tparam Int the integral type of the std::integer_sequence
		 * @tparam FIRST the starting integer
		 * @tparam LAST the end integer
		 */
		template<typename Int, Int FIRST, Int LAST, Int ...IXS>
		consteval auto make_integer_sequence(std::integer_sequence<Int, IXS...> = {}) {
			std::integer_sequence<Int, IXS..., FIRST> const acc;

			if constexpr (FIRST == LAST) {
				return acc;
			} else if constexpr (FIRST < LAST) {
				return make_integer_sequence<Int, FIRST + 1, LAST>(acc);
			} else {
				return make_integer_sequence<Int, FIRST - 1, LAST>(acc);
			}
		}

		/**
		 * Generates the actual std::variant type
		 * for an IntegralTemplatedTuple<FIRST, LAST, T> where Ixs = 0, 1, ..., LAST - FIRST
		 * aka std::variant<T<FIRST + 0>, T<FIRST + 1>, ...>.
		 *
		 * Note: This function does not have an implementation because it is only used in decltype context.
		 */
		template<template<std::integral auto> typename T, typename Int, Int ...IXS>
		std::variant<T<IXS>...> make_itv_type_impl(std::integer_sequence<Int, IXS...>);

		/**
		 * Generates the std::variant type corresponding to
		 * integral_templated_variant<FIRST, LAST, T> by calling make_itv_type_impl with the correct index_sequence.
		 *
		 * Note: only callable in decltype context
		 */
		template<std::integral auto FIRST, std::integral auto LAST, template<std::integral auto> typename T>
		consteval auto make_itv_type() {
			using integral_t = std::common_type_t<decltype(FIRST), decltype(LAST)>;
			return make_itv_type_impl<T>(make_integer_sequence<integral_t, FIRST, LAST>());
		}

		template<std::integral auto Min, std::integral auto Max, template<std::integral auto> typename T>
		using itv_type_t = std::invoke_result_t<decltype(make_itv_type<Min, Max, T>)>;
	} // namespace itv_detail

	/**
	 * A wrapper type for std::variant guarantees to only contain variants
	 * of the form T<IX>. Where IX is in FIRST..LAST (inclusive).
	 *
	 * @tparam FIRST the first IX for T<IX>
	 * @tparam LAST the last IX for T<IX>
	 * @tparam T the template that gets instantiated with T<IX> for IX in FIRST..LAST (inclusive)
	 */
	template<template<std::integral auto> typename T, std::integral auto FIRST, std::integral auto LAST>
	struct integral_templated_variant {
		static constexpr std::integral auto MIN = std::min(FIRST, LAST);
		static constexpr std::integral auto MAX = std::max(FIRST, LAST);

		template<std::integral auto IX>
		using value_type = T<IX>;

	private:
		using variant_t = itv_detail::itv_type_t<MIN, MAX, T>;
		variant_t repr;

		template<std::integral auto IX>
		static consteval void check_ix() {
			static_assert(IX >= MIN && IX <= MAX, "Index out of range");
		}

	public:
		constexpr integral_templated_variant(integral_templated_variant const &other)
			: repr{other.repr} {
		}

		constexpr integral_templated_variant(integral_templated_variant &&other)
			: repr{std::move(other.repr)} {
		}

		template<std::integral auto IX>
		constexpr integral_templated_variant(T<IX> const &value) noexcept(std::is_nothrow_copy_constructible_v<T<IX>>)
			: repr{value} {
			check_ix<IX>();
		}

		template<std::integral auto IX>
		constexpr integral_templated_variant(T<IX> &&value) noexcept(std::is_nothrow_move_constructible_v<T<IX>>)
			: repr{std::move(value)} {
			check_ix<IX>();
		}

		template<typename U, typename ...Args>
		explicit constexpr integral_templated_variant(std::in_place_type_t<U>, Args &&...args)
			: repr{std::in_place_type<U>, std::forward<Args>(args)...} {
		}

		template<std::integral auto IX>
		friend constexpr T<IX> &get(integral_templated_variant &variant) {
			check_ix<IX>();
			return std::get<T<IX>>(variant.repr);
		}

		template<std::integral auto IX>
		friend constexpr T<IX> const &get(integral_templated_variant const &variant) {
			check_ix<IX>();
			return std::get<T<IX>>(variant.repr);
		}

		template<std::integral auto IX>
		friend constexpr T<IX> &&get(integral_templated_variant &&variant) {
			check_ix<IX>();
			return std::get<T<IX>>(std::move(variant.repr));
		}

		template<std::integral auto IX>
		friend constexpr T<IX> const &&get(integral_templated_variant const &&variant) {
			check_ix<IX>();
			return std::get<T<IX>>(static_cast<variant_t const &&>(variant.repr));
		}

		template<typename Visitor, typename ...ITVariants>
		friend constexpr decltype(auto) visit(Visitor &&visitor, ITVariants &&...vars);
	};

	template<typename Visitor, typename ...ITVariants>
	constexpr decltype(auto) visit(Visitor &&visitor, ITVariants &&...vars) {
		return std::visit(std::forward<Visitor>(visitor), std::forward<ITVariants>(vars).repr...);
	}

} // namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATED_VARIANT_HPP
