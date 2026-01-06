#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP

#include <dice/template-library/integral_sequence.hpp>
#include <dice/template-library/lazy_conditional.hpp>
#include <dice/template-library/type_list.hpp>
#include <dice/template-library/type_traits.hpp>

#include <algorithm>
#include <concepts>
#include <utility>
#include <variant>

namespace dice::template_library {

	namespace detail_itv2 {
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using variant_provider = type_list::apply<detail_integral_template_util::make_type_list<first, last, T>, std::variant>;

		/**
		 * Empty variants are std::variant<std::monostate>.
		 */
		struct empty_variant_provider {
			using type = std::variant<std::monostate>;
		};

		/**
		 * Generates a std::variant<T<ix>...> from a type_list<T<ix>...>
		 * Empty sequences produce std::variant<std::monostate>
		 */
		template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_variant = lazy_conditional<first == last, empty_variant_provider, variant_provider<first, last, T>>::type;
	} // namespace detail_itv2

	/**
	 * A std::variant-like type that holds variants T<ix> for each ix in the sequence
	 *
	 * Direction is automatic:
	 * - first < last: ascending [first, last) → T<first> .. T<last-1>
	 * - first > last: descending (last, first] → T<first> .. T<last+1>
	 * - first == last: std::variant<std::monostate> (empty)
	 *
	 * @tparam first the first ix for T<ix> (inclusive)
	 * @tparam last the last ix (exclusive, not included in the variant)
	 * @tparam T the template that gets instantiated with T<ix>
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct integral_template_variant_v2 {
		using index_type = decltype(first);
		using underlying_type = detail_itv2::make_variant<first, last, T>;

		template<index_type ix>
		using value_type = T<ix>;

	private:
		template<index_type ix>
		static constexpr void check_ix() {
			static_assert(detail_integral_template_util::valid_index_v<first, last, ix>, "Index out of range");
		}

		underlying_type repr_;

	public:
		constexpr integral_template_variant_v2() noexcept(std::is_nothrow_default_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant_v2(integral_template_variant_v2 const &other) noexcept(std::is_copy_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant_v2(integral_template_variant_v2 &&other) noexcept(std::is_nothrow_move_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant_v2 &operator=(integral_template_variant_v2 const &other) noexcept(std::is_nothrow_copy_assignable_v<underlying_type>) = default;
		constexpr integral_template_variant_v2 &operator=(integral_template_variant_v2 &&other) noexcept(std::is_nothrow_move_assignable_v<underlying_type>) = default;
		constexpr ~integral_template_variant_v2() noexcept(std::is_nothrow_destructible_v<underlying_type>) = default;

		template<index_type ix>
		constexpr integral_template_variant_v2(T<ix> const &value) noexcept(std::is_nothrow_copy_constructible_v<T<ix>>)
			: repr_{value} {
			check_ix<ix>();
		}

		template<index_type ix>
		constexpr integral_template_variant_v2(T<ix> &&value) noexcept(std::is_nothrow_move_constructible_v<T<ix>>)
			: repr_{std::move(value)} {
			check_ix<ix>();
		}

		template<typename U, typename ...Args>
		explicit constexpr integral_template_variant_v2(std::in_place_type_t<U>, Args &&...args) noexcept(std::is_nothrow_constructible_v<U, decltype(std::forward<Args>(args))...>)
			: repr_{std::in_place_type<U>, std::forward<Args>(args)...} {
		}

		template<index_type ix, typename ...Args>
		constexpr value_type<ix> &emplace(Args &&...args) {
			check_ix<ix>();
			return repr_.template emplace<T<ix>>(std::forward<Args>(args)...);
		}

		[[nodiscard]] constexpr index_type index() const noexcept {
			if constexpr (first < last) {
				// Ascending
				return static_cast<index_type>(first + repr_.index());
			} else {
				// Descending
				return static_cast<index_type>(first - repr_.index());
			}
		}

		template<index_type ix, typename Self>
		constexpr decltype(auto) get(this Self &&self) {
			check_ix<ix>();
			return std::get<T<ix>>(dice::template_library::forward_like<Self>(self.repr_));
		}

		template<typename Self, typename Visitor>
		constexpr decltype(auto) visit(this Self &&self, Visitor &&visitor) {
			return std::visit(std::forward<Visitor>(visitor), dice::template_library::forward_like<Self>(self.repr_));
		}

		template<typename Self>
		[[nodiscard]] constexpr decltype(auto) to_underlying(this Self &&self) noexcept {
			return dice::template_library::forward_like<Self>(self.repr_);
		}

		template<index_type ix>
		[[nodiscard]] constexpr bool holds_alternative() const noexcept {
			check_ix<ix>();
			return std::holds_alternative<T<ix>>(repr_);
		}

		template<typename U>
		[[nodiscard]] constexpr bool holds_alternative() const noexcept {
			return std::holds_alternative<U>(repr_);
		}

		constexpr auto operator<=>(integral_template_variant_v2 const &other) const noexcept = default;
		constexpr bool operator==(integral_template_variant_v2 const &other) const noexcept = default;
	};

}// namespace dice::template_library

template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
struct std::hash<::dice::template_library::integral_template_variant_v2<first, last, T>> {
	[[nodiscard]] size_t operator()(::dice::template_library::integral_template_variant_v2<first, last, T> const &variant) const noexcept {
		using underlying_type = typename ::dice::template_library::integral_template_variant_v2<first, last, T>::underlying_type;
		return std::hash<underlying_type>{}(variant.to_underlying());
	}
};

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP
