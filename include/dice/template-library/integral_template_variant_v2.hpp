#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP

#include <dice/template-library/integral_template_common.hpp>
#include <dice/template-library/type_traits.hpp>

#include <algorithm>
#include <concepts>
#include <utility>
#include <variant>

namespace dice::template_library {

	namespace itv_detail_v2 {
		using namespace it_detail_v2;
		/**
		 * Generates a std::variant<T<ix>...> based on direction
		 * from a type_list<T<ix>...>
		 */
		template<direction Dir, std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
		using make_variant = type_list::apply_t<make_type_list<Dir, first, last, T>, std::variant>;
	}// namespace itv_detail_v2

	/**
	 * A std::variant-like type that holds variants based on direction
	 * - ascending: T<first> .. T<last-1> (exclusive upper bound)
	 * - descending: T<first> .. T<last+1> (exclusive lower bound)
	 *
	 * @tparam first the first ix for T<ix> (inclusive)
	 * @tparam last the last ix (exclusive, not included in the variant)
	 * @tparam T the template that gets instantiated with T<ix>
	 * @tparam Dir direction of the sequence (ascending or descending)
	 *
	 * @note For ascending: first must be less than last. For descending: first must be greater than last.
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T,
			 direction Dir = direction::ascending>
	struct integral_template_variant_v2 {
		static_assert((Dir == direction::ascending && first < last) ||
							  (Dir == direction::descending && first > last),
					  "Invalid first/last combination for direction");

		using index_type = decltype(first);
		using underlying_type = itv_detail_v2::make_variant<Dir, first, last, T>;

		template<index_type ix>
		using value_type = T<ix>;

	private:
		template<index_type ix>
		static constexpr bool check_ix_v = it_detail_v2::valid_index_v<Dir, first, last, ix>;

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
			(void) check_ix_v<ix>;
		}

		template<index_type ix>
		constexpr integral_template_variant_v2(T<ix> &&value) noexcept(std::is_nothrow_move_constructible_v<T<ix>>)
			: repr_{std::move(value)} {
			(void) check_ix_v<ix>;
		}

		template<typename U, typename... Args>
		explicit constexpr integral_template_variant_v2(std::in_place_type_t<U>, Args &&...args) noexcept(std::is_nothrow_constructible_v<U, decltype(std::forward<Args>(args))...>)
			: repr_{std::in_place_type<U>, std::forward<Args>(args)...} {
		}

		template<index_type ix, typename... Args>
		constexpr explicit integral_template_variant_v2(std::in_place_index_t<ix>, Args &&...args)
			: repr_(std::in_place_type<T<ix>>, std::forward<Args>(args)...) {
			(void) check_ix_v<ix>;
		}

		template<index_type ix, typename... Args>
		constexpr value_type<ix> &emplace(Args &&...args) {
			(void) check_ix_v<ix>;
			return repr_.template emplace<T<ix>>(std::forward<Args>(args)...);
		}

		[[nodiscard]] static constexpr size_t size() noexcept {
			return std::variant_size_v<underlying_type>;
		}

		[[nodiscard]] constexpr index_type index() const noexcept {
			return static_cast<index_type>(first + (repr_.index() * static_cast<int8_t>(Dir)));
		}

		template<index_type ix, typename Self>
		constexpr decltype(auto) get(this Self &&self) {
			(void) check_ix_v<ix>;
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

		constexpr auto operator<=>(integral_template_variant_v2 const &other) const noexcept = default;
	};

	/**
	 * A std::variant-like type that holds variants of T<first> .. T<last+1> (exclusive lower bound, descending)
	 *
	 * @tparam first the first ix for T<ix> (inclusive)
	 * @tparam last the last ix (exclusive, not included in the variant)
	 * @tparam T the template that gets instantiated with T<ix> for ix in (last, first]
	 *
	 * @note first must be greater than last.
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	using integral_template_variant_rev_v2 = integral_template_variant_v2<first, last, T, direction::descending>;

	template<std::integral auto index, decltype(index) first, decltype(index) last, template<decltype(index)> typename T,
			 direction Dir>
	constexpr bool holds_alternative(integral_template_variant_v2<first, last, T, Dir> const &variant) noexcept {
		return std::holds_alternative<T<index>>(variant.to_underlying());
	}

	template<typename U, std::integral auto first, decltype(first) last, template<decltype(first)> typename T,
			 direction Dir>
	constexpr bool holds_alternative(integral_template_variant_v2<first, last, T, Dir> const &variant) noexcept {
		return std::holds_alternative<U>(variant.to_underlying());
	}
}// namespace dice::template_library

namespace std {
	template<integral auto first, decltype(first) last, template<decltype(first)> typename T,
			 ::dice::template_library::direction Dir>
	struct hash<::dice::template_library::integral_template_variant_v2<first, last, T, Dir>> {
		[[nodiscard]] size_t operator()(::dice::template_library::integral_template_variant_v2<first, last, T, Dir> const &variant) const noexcept {
			using underlying_type = typename ::dice::template_library::integral_template_variant_v2<first, last, T, Dir>::underlying_type;
			return hash<underlying_type>{}(variant.to_underlying());
		}
	};
}// namespace std

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP
