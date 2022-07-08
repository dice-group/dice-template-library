#ifndef DICE_TEMPLATE_LIBRARY_FOR_HPP
#define DICE_TEMPLATE_LIBRARY_FOR_HPP

#include <type_traits>
#include <utility>

namespace dice::template_library {
	/**
	 * Call a templated lambda with each of the provided types Ts...
	 * @tparam Ts The types with which the lambda should be called.
	 * @tparam F Type of the lambda
	 * @param f the lambda.
	 */
	template<typename... Ts, typename F>
	constexpr void for_types(F &&f) {
		(f.template operator()<Ts>(), ...);
	}

	namespace for_values_detail {
		template<typename F>
		struct cont {
			template<auto FIRST, auto... TAIL>
			static constexpr void for_values_consume(F &&f) {
				(f(std::integral_constant<decltype(FIRST), FIRST>()));
				if constexpr (sizeof...(TAIL) > 0)
					for_values_consume<TAIL...>(std::forward<F>(f));
			}
		};
	}// namespace for_values_detail

	/**
	 * Call a lambda with a single templated argument with each provided argument Xs...
	 * @tparam Xs For each element of Xs the function is called once.
	 * @tparam F Type of the lambda
	 * @param f The lambda
	 */
	template<auto... Xs, typename F>
	constexpr void for_values(F &&f) {
		if constexpr (sizeof...(Xs) > 0)
			for_values_detail::cont<F>::template for_values_consume<Xs...>(std::forward<F>(f));
	}

	/**
	  * Call a lambda with a single templated argument from an integral range.
	  * @tparam BEGIN start of the range
	  * @tparam END end of the range (exclusive)
	  * @tparam F Type of the lambda
	  * @param f The lambda
	  */
	template<std::integral auto BEGIN, std::integral auto END, typename F>
	constexpr void for_range(F &&f) {
		using t = std::common_type_t<std::decay_t<decltype(BEGIN)>, std::decay_t<decltype(END)>>;

		if constexpr (t(BEGIN) <= t(END)) {
			[&f]<auto... Xs>(std::integer_sequence<t, Xs...>) {
				for_values<(t(BEGIN) + t(Xs))...>(f);
			}
			(std::make_integer_sequence<t, t(END) - t(BEGIN)>{});
		} else {
			[&f]<auto... Xs>(std::integer_sequence<t, Xs...>) {
				for_values<(t(BEGIN) - t(Xs))...>(f);
			}
			(std::make_integer_sequence<t, t(BEGIN) - t(END)>{});
		}
	}
}// namespace dice::template_library
#endif//DICE_TEMPLATE_LIBRARY_FOR_HPP
