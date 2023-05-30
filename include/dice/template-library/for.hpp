#ifndef DICE_TEMPLATE_LIBRARY_FOR_HPP
#define DICE_TEMPLATE_LIBRARY_FOR_HPP

#include <functional>
#include <type_traits>
#include <utility>

namespace dice::template_library {
	/**
	 * Call a templated lambda with each of the provided types Ts...
	 * @tparam Ts The types with which the lambda should be called.
	 * @tparam F Type of the lambda
	 * @param f the lambda.
	 */
	template<typename ...Ts, typename F>
	constexpr void for_types(F &&f) noexcept((noexcept(f.template operator()<Ts>()) && ...)) {
		(f.template operator()<Ts>(), ...);
	}

	/**
	 * Call a lambda with a single templated argument with each provided argument xs...
	 * @tparam xs For each element of Xs the function is called once.
	 * @tparam F Type of the lambda
	 * @param f The lambda
	 */
	template<auto ...xs, typename F>
	constexpr void for_values(F &&f) noexcept((std::is_nothrow_invocable_v<F, std::integral_constant<decltype(xs), xs>> && ...)) {
		(std::invoke(f, std::integral_constant<decltype(xs), xs>{}), ...);
	}

	/**
	  * Call a lambda with a single templated argument from an integral range.
	  * @tparam first start of the range
	  * @tparam end end of the range (exclusive)
	  * @tparam F Type of the lambda
	  * @param f The lambda
	  *
	  * @note first is allowed to be greater than last
	  */
	template<std::integral auto first, decltype(first) last, typename F>
	constexpr void for_range(F &&f) {
		if constexpr (first < last) {
			auto const impl = [&f]<auto... xs>(std::integer_sequence<decltype(first), xs...>) {
				for_values<(first + xs)...>(std::forward<F>(f));
			};

			impl(std::make_integer_sequence<decltype(first), last - first>{});
		} else {
			auto const impl = [&f]<auto... xs>(std::integer_sequence<decltype(first), xs...>) {
				for_values<(first - xs)...>(std::forward<F>(f));
			};

			impl(std::make_integer_sequence<decltype(first), first - last>{});
		}
	}
}// namespace dice::template_library
#endif//DICE_TEMPLATE_LIBRARY_FOR_HPP
