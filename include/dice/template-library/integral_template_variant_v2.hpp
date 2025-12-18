#ifndef DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP
#define DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP

#include <dice/template-library/integral_template_common.hpp>

#include <algorithm>
#include <concepts>
#include <utility>
#include <variant>

namespace dice::template_library {

	namespace itv_detail_v2 {
		using namespace integral_template_detail;

		/**
		 * Generates the actual std::variant type
		 * for an integral_template_variant_v2<first, last, T> where Ixs = first, first+1, ..., last-1
		 * aka std::variant<T<first>, T<first + 1>, ..., T<last-1>>.
		 *
		 * Note: This function does not have an implementation because it is only used in decltype context.
		 */
		template<std::integral Int, template<Int> typename T, Int... ixs>
		std::variant<T<ixs>...> make_itv_type_impl(std::integer_sequence<Int, ixs...>);

		/**
		 * Generates the std::variant type corresponding to
		 * integral_template_variant_v2<first, last, T> by calling make_itv_type_impl with the correct index_sequence.
		 *
		 * Note: only callable in decltype context
		 */
		template<std::integral Int, Int first, Int last, template<Int> typename T>
		auto make_itv_type_asc() {
			return make_itv_type_impl<Int, T>(make_integer_sequence_asc<Int, first, last>());
		}

		template<std::integral Int, Int first, Int last, template<Int> typename T>
		using itv_type_asc_t = std::invoke_result_t<decltype(make_itv_type_asc<Int, first, last, T>)>;

		/**
		 * Generates the std::variant type corresponding to
		 * integral_template_variant_rev_v2<first, last, T> by calling make_itv_type_impl with the correct index_sequence.
		 *
		 * Note: only callable in decltype context
		 */
		template<std::integral Int, Int first, Int last, template<Int> typename T>
		auto make_itv_type_desc() {
			return make_itv_type_impl<Int, T>(make_integer_sequence_desc<Int, first, last>());
		}

		template<std::integral Int, Int first, Int last, template<Int> typename T>
		using itv_type_desc_t = std::invoke_result_t<decltype(make_itv_type_desc<Int, first, last, T>)>;
	}// namespace itv_detail_v2

	/**
	 * A std::variant-like type that holds variants of T<first> .. T<last-1> (exclusive upper bound)
	 *
	 * @tparam first the first ix for T<ix> (inclusive)
	 * @tparam last the last ix (exclusive, not included in the variant)
	 * @tparam T the template that gets instantiated with T<ix> for ix in [first, last)
	 *
	 * @note first must be less than last.
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct integral_template_variant_v2 {
		static_assert(first < last, "integral_template_variant_v2 requires first < last.");

		using index_type = decltype(first);
		using underlying_type = itv_detail_v2::itv_type_asc_t<index_type, first, last, T>;

		template<index_type ix>
		using value_type = T<ix>;

	private:
		underlying_type repr_;

		template<index_type ix>
		static consteval void check_ix() {
			static_assert(ix >= first && ix < last, "Index out of range");
		}

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

		template<typename U, typename... Args>
		explicit constexpr integral_template_variant_v2(std::in_place_type_t<U>, Args &&...args) noexcept(std::is_nothrow_constructible_v<U, decltype(std::forward<Args>(args))...>)
			: repr_{std::in_place_type<U>, std::forward<Args>(args)...} {
		}

		[[nodiscard]] static constexpr size_t size() noexcept {
			return std::variant_size_v<underlying_type>;
		}

		[[nodiscard]] constexpr index_type index() const noexcept {
			return first + static_cast<index_type>(repr_.index());
		}

		template<index_type ix, typename Self>
		constexpr decltype(auto) get(this Self &&self) {
			check_ix<ix>();
			return std::get<T<ix>>(std::forward<Self>(self).repr_);
		}

		template<typename Self, typename Visitor>
		constexpr decltype(auto) visit(this Self &&self, Visitor &&visitor) {
			return std::visit(std::forward<Visitor>(visitor), std::forward<Self>(self).repr_);
		}

		template<typename Self>
		[[nodiscard]] constexpr decltype(auto) to_underlying(this Self &&self) noexcept {
			return std::forward<Self>(self).repr_;
		}

		constexpr auto operator<=>(integral_template_variant_v2 const &other) const noexcept = default;
	};

	template<std::integral auto index, decltype(index) first, decltype(index) last, template<decltype(index)> typename T>
	constexpr bool holds_alternative(integral_template_variant_v2<first, last, T> const &variant) noexcept {
		return std::holds_alternative<T<index>>(variant.to_underlying());
	}

	template<typename U, std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	constexpr bool holds_alternative(integral_template_variant_v2<first, last, T> const &variant) noexcept {
		return std::holds_alternative<U>(variant.to_underlying());
	}

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
	struct integral_template_variant_rev_v2 {
		static_assert(first > last, "integral_template_variant_rev_v2 requires first > last.");

		using index_type = decltype(first);
		using underlying_type = itv_detail_v2::itv_type_desc_t<index_type, first, last, T>;

		template<index_type ix>
		using value_type = T<ix>;

	private:
		underlying_type repr_;

		template<index_type ix>
		static consteval void check_ix() {
			static_assert(ix <= first && ix > last, "Index out of range");
		}

		static constexpr index_type make_index(index_type ix) {
			return first - ix;
		}

	public:
		constexpr integral_template_variant_rev_v2() noexcept(std::is_nothrow_default_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant_rev_v2(integral_template_variant_rev_v2 const &other) noexcept(std::is_copy_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant_rev_v2(integral_template_variant_rev_v2 &&other) noexcept(std::is_nothrow_move_constructible_v<underlying_type>) = default;
		constexpr integral_template_variant_rev_v2 &operator=(integral_template_variant_rev_v2 const &other) noexcept(std::is_nothrow_copy_assignable_v<underlying_type>) = default;
		constexpr integral_template_variant_rev_v2 &operator=(integral_template_variant_rev_v2 &&other) noexcept(std::is_nothrow_move_assignable_v<underlying_type>) = default;
		constexpr ~integral_template_variant_rev_v2() noexcept(std::is_nothrow_destructible_v<underlying_type>) = default;

		template<index_type ix>
		constexpr integral_template_variant_rev_v2(T<ix> const &value) noexcept(std::is_nothrow_copy_constructible_v<T<ix>>)
			: repr_{value} {
			check_ix<ix>();
		}

		template<index_type ix>
		constexpr integral_template_variant_rev_v2(T<ix> &&value) noexcept(std::is_nothrow_move_constructible_v<T<ix>>)
			: repr_{std::move(value)} {
			check_ix<ix>();
		}

		template<typename U, typename... Args>
		explicit constexpr integral_template_variant_rev_v2(std::in_place_type_t<U>, Args &&...args) noexcept(std::is_nothrow_constructible_v<U, decltype(std::forward<Args>(args))...>)
			: repr_{std::in_place_type<U>, std::forward<Args>(args)...} {
		}

		template<index_type ix, typename... Args>
		constexpr explicit integral_template_variant_rev_v2(std::in_place_index_t<ix>, Args &&...args) : repr_(std::in_place_type<T<ix>>, std::forward<Args>(args)...) {
			check_ix<ix>();
		}

		template<index_type ix, typename... Args>
		constexpr value_type<ix> &emplace(Args &&...args) {
			check_ix<ix>();
			return repr_.template emplace<T<ix>>(std::forward<Args>(args)...);
		}

		constexpr index_type index() const noexcept {
			return first - static_cast<index_type>(repr_.index());
		}

		template<index_type ix, typename Self>
		constexpr decltype(auto) get(this Self &&self) {
			check_ix<ix>();
			return std::get<T<ix>>(std::forward<Self>(self).repr_);
		}

		template<typename Self, typename Visitor>
		constexpr decltype(auto) visit(this Self &&self, Visitor &&visitor) {
			return std::visit(std::forward<Visitor>(visitor), std::forward<Self>(self).repr_);
		}

		template<typename Self>
		[[nodiscard]] constexpr decltype(auto) to_underlying(this Self &&self) noexcept {
			return std::forward<Self>(self).repr_;
		}

		constexpr auto operator<=>(integral_template_variant_rev_v2 const &other) const noexcept = default;
	};

	template<std::integral auto index, decltype(index) first, decltype(index) last, template<decltype(index)> typename T>
	constexpr bool holds_alternative(integral_template_variant_rev_v2<first, last, T> const &variant) noexcept {
		return std::holds_alternative<T<index>>(variant.to_underlying());
	}

	template<typename U, std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	constexpr bool holds_alternative(integral_template_variant_rev_v2<first, last, T> const &variant) noexcept {
		return std::holds_alternative<U>(variant.to_underlying());
	}
}// namespace dice::template_library

namespace std {
	template<integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct hash<::dice::template_library::integral_template_variant_v2<first, last, T>> {
		[[nodiscard]] size_t operator()(::dice::template_library::integral_template_variant_v2<first, last, T> const &variant) const noexcept {
			using underlying_type = typename ::dice::template_library::integral_template_variant_v2<first, last, T>::underlying_type;
			return hash<underlying_type>{}(variant.to_underlying());
		}
	};

	template<integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct hash<::dice::template_library::integral_template_variant_rev_v2<first, last, T>> {
		[[nodiscard]] size_t operator()(::dice::template_library::integral_template_variant_rev_v2<first, last, T> const &variant) const noexcept {
			using underlying_type = typename ::dice::template_library::integral_template_variant_rev_v2<first, last, T>::underlying_type;
			return hash<underlying_type>{}(variant.to_underlying());
		}
	};
}// namespace std

#endif//DICE_TEMPLATE_LIBRARY_INTEGRAL_TEMPLATE_VARIANT_V2_HPP
