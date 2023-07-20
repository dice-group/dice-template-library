#ifndef HYPERTRIE_SWITCHTEMPLATEFUNCTIONS_HPP
#define HYPERTRIE_SWITCHTEMPLATEFUNCTIONS_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <functional>

namespace dice::template_library {

	namespace detail_switch_cases {
		template<std::integral auto i, decltype(i) max, decltype(i) min = i, typename F>
		constexpr decltype(auto) execute_case(decltype(i) value, F &&cases_function) {
			if constexpr (i < ((max == min) ? max : max - 1)) {
				if (value != i) {
					return execute_case<i + 1, max, min>(value, std::forward<F>(cases_function));
				}
			}

			static_assert(i < max);
			return std::invoke(std::forward<F>(cases_function), std::integral_constant<decltype(i), i>{});
		}

		/**
		 * This function mimics the return type of the function given to `switch_cases`.
		 * It will always fail (via assert).
		 * It can be used as the default function for `switch_cases`, if the default should never be reached.
		 * @tparam F The type of the function to mimic.
		 * @tparam NUM Any number in the valid range (see `RType`).
		 * @return Nothing.
		 */
		template<typename F, auto NUM>
		std::invoke_result_t<F, std::integral_constant<decltype(NUM), NUM>> unreachable() noexcept {
			assert(false);
			__builtin_unreachable();
		}
	}// namespace detail_switch_cases

	/**
   * Generates a switch-case function at compile-time which is evaluated at runtime. The switch is a lambda like: `[&](auto i_t){ ... }`.
   * `i_t` can be used as template parameter. It will be instantiated with the values between first and last excluding
   * the maximum value. If any of the <div>cases_function</div>s returns all of them must return. The types of the
   * returned objects must be compatible. This applies also to the default_function.
   * @tparam first first value of the integer range which is switched over. It doesn't matter if first or last is
   * larger. The upper bound is not excluded, i.e. [min,max)
   * @tparam last last value of the integer range which is switched over
   * @tparam F automatically deduced type of switch function
   * @tparam D automatically deduced type of default_function
   * @param condition the switch condition
   * @param cases_function the switch function template or lambda. See function description for details.
   * @param default_function the default fallback if condition is out of range [min,max) spanned b first, last
   * @return The value returned from the switch case
   */
	template<std::integral auto first, decltype(first) last, typename F, typename D>
	constexpr decltype(auto) switch_cases(decltype(first) condition, F &&cases_function, D &&default_function) {
		using namespace detail_switch_cases;

		constexpr decltype(first) min = std::min(first, last);
		constexpr decltype(last) max = std::max(first, last);

		if constexpr (min != max) {
			if (min <= condition && condition < max) {
				return execute_case<min, max>(condition, std::forward<F>(cases_function));
			}
		}

		return std::invoke(std::forward<D>(default_function));
	}

	/**
	 * Overload for `switch_cases`.
	 * It will simply set the `default_function` to an always failing function that should never be reached.
	 * Could also be done via a default value for the `default_function` parameter,
	 * but the templated type makes it pretty ugly.
	 * @tparam first first value of the integer range which is switched over. It doesn't matter if first or last is
	 * larger. The upper bound is not excluded, i.e. [min,max)
	 * @tparam last last value of the integer range which is switched over
	 * @tparam F automatically deduced type of switch function
	 * @tparam D automatically deduced type of default_function
	 * @param condition the switch condition
	 * @param cases_function the switch function template or lambda. See function description for details.
	 * @param default_function the default fallback if condition is out of range [min,max) spanned b first, last
	 * @return The value returned from the switch case
	 */
	template<std::integral auto first, decltype(first) last, typename F>
	constexpr decltype(auto) switch_cases(decltype(first) condition, F &&cases_function) {
		return switch_cases<first, last>(condition, std::forward<F>(cases_function), detail_switch_cases::unreachable<F, first>);
	}

	/**
	 * Generates a switch-case function at compile-time which is evaluated at runtime. The switch is a lambda like: `[&](auto i_t){ ... }`.
	 * `i_t` can be used as template parameter. It will be instantiated with the values between first and last excluding
	 * the maximum value. First is fixed to 0. If any of the <div>cases_function</div>s returns all of them must return. The types of the
	 * returned objects must be compatible. This applies also to the default_function.
	 * @tparam last last value of the integer range which is switched over
	 * @tparam F automatically deduced type of switch function
	 * @tparam D automatically deduced type of default_function
	 * @param condition the switch condition
	 * @param cases_function the switch function template or lambda. See function description for details.
	 * @param default_function the default fallback if condition is out of range [min,max) spanned b first, last
	 * @return The value returned from the switch case
	 */
	template<std::integral auto last, typename F, typename D>
	constexpr decltype(auto) switch_cases(decltype(last) condition, F &&cases_function, D &&default_function) {
		return switch_cases<decltype(last){}, last>(condition, std::forward<F>(cases_function), std::forward<D>(default_function));
	}


	/**
	* Overload for `switch_cases`.
	* It will simply set the `default_function` to an always failing function that should never be reached.
	* Could also be done via a default value for the `default_function` parameter,
	* but the templated type makes it pretty ugly.
	* @tparam last last value of the integer range which is switched over
	* @tparam F automatically deduced type of switch function
	* @tparam D automatically deduced type of default_function
	* @param condition the switch condition
	* @param cases_function the switch function template or lambda. See function description for details.
	* @param default_function the default fallback if condition is out of range [min,max) spanned b first, last
	* @return The value returned from the switch case
	 */
	template<std::integral auto last, typename F>
	constexpr decltype(auto) switch_cases(decltype(last) condition, F &&cases_function) {
		return switch_cases<decltype(last){}, last>(condition, std::forward<F>(cases_function), detail_switch_cases::unreachable<F, last>);
	}

	/**
	 * Similar to switch-cases except for switching on booleans
	 * @tparam F automatically deduced type of switch function
	 * @param condition the switch condition
	 * @param cases_function the switch function template or lambda
	 * @return The value returned from the cases_function
	 */
	template<typename F>
	constexpr decltype(auto) switch_bool(bool condition, F &&cases_function) {
		if (condition) {
			return std::invoke(std::forward<F>(cases_function), std::bool_constant<true>{});
		}

		return std::invoke(std::forward<F>(cases_function), std::bool_constant<false>{});
	}
}// namespace dice::template_library

#endif//HYPERTRIE_SWITCHTEMPLATEFUNCTIONS_HPP
