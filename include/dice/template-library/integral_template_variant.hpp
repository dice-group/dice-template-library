#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_HPP

#include <algorithm>
#include <concepts>
#include <utility>
#include <variant>

namespace dice::template_library {

	namespace itv_detail {
		/**
		 * generates the std::integer_sequence<Int, first, ..., last>
		 * @note first is allowed to be greater than last
		 *
		 * @tparam Int the integral type of the std::integer_sequence
		 * @tparam first the starting integer
		 * @tparam last the end integer
		 */
		template<std::integral Int, Int first, Int last, Int ... ixs>
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
		 * Generates the actual std::variant type
		 * for an IntegralTemplatedTuple<first, last, T> where Ixs = 0, 1, ..., last - first
		 * aka std::variant<T<first + 0>, T<first + 1>, ...>.
		 *
		 * Note: This function does not have an implementation because it is only used in decltype context.
		 */
		template<std::integral Int, template<Int> typename T, Int ...ixs>
		std::variant<T<ixs>...> make_itv_type_impl(std::integer_sequence<Int, ixs...>);

		/**
		 * Generates the std::variant type corresponding to
		 * integral_template_variant<first, last, T> by calling make_itv_type_impl with the correct index_sequence.
		 *
		 * Note: only callable in decltype context
		 */
		template<std::integral Int, Int first, Int last, template<Int> typename T>
		auto make_itv_type() {
			return make_itv_type_impl<Int, T>(make_integer_sequence<Int, first, last>());
		}

		template<std::integral Int, Int first, Int last, template<Int> typename T>
		using itv_type_t = std::invoke_result_t<decltype(make_itv_type<Int, first, last, T>)>;
	} // namespace itv_detail

	/**
	 * A std::variant-like type that holds variants of T<first> .. T<last> (inclusive)
	 *
	 * @tparam first the first ix for T<ix>
	 * @tparam last the last ix for T<ix>
	 * @tparam T the template that gets instantiated with T<ix> for ix in first..last (inclusive)
	 *
	 * @note first is allowed to be smaller than last.
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct integral_template_variant {
		using index_type = decltype(first);

		template<index_type ix>
		using value_type = T<ix>;

	private:
		using underlying_type = itv_detail::itv_type_t<index_type, first, last, T>;
		underlying_type repr_;

		template<index_type ix>
		static consteval void check_ix() {
			static_assert(ix >= std::min(first, last) && ix <= std::max(first, last), "Index out of range");
		}

	public:
		constexpr integral_template_variant() noexcept(std::is_nothrow_default_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant(integral_template_variant const &other) noexcept(std::is_copy_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant(integral_template_variant &&other) noexcept(std::is_nothrow_move_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant &operator=(integral_template_variant const &other) noexcept(std::is_nothrow_copy_assignable_v<underlying_type>) = default;
		constexpr integral_template_variant &operator=(integral_template_variant &&other) noexcept(std::is_nothrow_move_assignable_v<underlying_type>) = default;
		constexpr ~integral_template_variant() noexcept(std::is_nothrow_destructible_v<underlying_type>) = default;

		template<index_type ix>
		constexpr integral_template_variant(T<ix> const &value) noexcept(std::is_nothrow_copy_constructible_v<T<ix>>)
			: repr_{value} {
			check_ix<ix>();
		}

		template<index_type ix>
		constexpr integral_template_variant(T<ix> &&value) noexcept(std::is_nothrow_move_constructible_v<T<ix>>)
			: repr_{std::move(value)} {
			check_ix<ix>();
		}

		template<typename U, typename ...Args>
		explicit constexpr integral_template_variant(std::in_place_type_t<U>, Args &&...args) noexcept(std::is_nothrow_constructible_v<U, decltype(std::forward<Args>(args))...>)
			: repr_{std::in_place_type<U>, std::forward<Args>(args)...} {
		}

		[[nodiscard]] static constexpr size_t size() noexcept {
			return std::variant_size_v<underlying_type>;
		}

		[[nodiscard]] constexpr index_type index() const noexcept {
			if constexpr (first < last) {
				return first + static_cast<index_type>(repr_.index());
			} else {
				return first - static_cast<index_type>(repr_.index());
			}
		}

		template<index_type ix>
		constexpr value_type<ix> const &get() const & {
			check_ix<ix>();
			return std::get<T<ix>>(repr_);
		}

		template<index_type ix>
		constexpr value_type<ix> &get() & {
			check_ix<ix>();
			return std::get<T<ix>>(repr_);
		}

		template<index_type ix>
		constexpr value_type<ix> const &&get() const && {
			check_ix<ix>();
			return std::get<T<ix>>(std::move(repr_));
		}

		template<index_type ix>
		constexpr value_type<ix> &&get() && {
			check_ix<ix>();
			return std::get<T<ix>>(std::move(repr_));
		}

		template<typename Visitor>
		constexpr decltype(auto) visit(Visitor &&visitor) & {
			return std::visit(std::forward<Visitor>(visitor), repr_);
		}

		template<typename Visitor>
		constexpr decltype(auto) visit(Visitor &&visitor) const & {
			return std::visit(std::forward<Visitor>(visitor), repr_);
		}

		template<typename Visitor>
		constexpr decltype(auto) visit(Visitor &&visitor) && {
			return std::visit(std::forward<Visitor>(visitor), std::move(repr_));
		}

		template<typename Visitor>
		constexpr decltype(auto) visit(Visitor &&visitor) const && {
			return std::visit(std::forward<Visitor>(visitor), std::move(repr_));
		}

		constexpr auto operator<=>(integral_template_variant const &other) const noexcept = default;
	};

	template<std::integral auto index, decltype(index) first, decltype(index) last, template<decltype(index)> typename T>
	constexpr bool holds_alternative(integral_template_variant<first, last, T> const &variant) noexcept {
		return std::holds_alternative<T<index>>(variant.to_underlying());
	}

	template<typename U, std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	constexpr bool holds_alternative(integral_template_variant<first, last, T> const &variant) noexcept {
		return std::holds_alternative<U>(variant.to_underlying());
	}
} // namespace dice::template_library

namespace std {
	template<integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct hash<::dice::template_library::integral_template_variant<first, last, T>> {
		[[nodiscard]] size_t operator()(::dice::template_library::integral_template_variant<first, last, T> const &variant) const noexcept {
			using underlying_type = typename ::dice::template_library::integral_template_variant<first, last, T>::underlying_type;
			return hash<underlying_type>{}(variant.to_underlying());
		}
	};
} // namespace std

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_HPP
