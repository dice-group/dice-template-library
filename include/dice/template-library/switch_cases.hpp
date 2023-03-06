#ifndef HYPERTRIE_SWITCHTEMPLATEFUNCTIONS_HPP
#define HYPERTRIE_SWITCHTEMPLATEFUNCTIONS_HPP

#include <array>
#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <utility>

namespace dice::template_library {

	namespace detail_switch_cases {

		template<typename T1, typename T2>
		struct common_integral;

		template<std::integral T>
		struct common_integral<T, T> {
			using type = T;
		};

		template<std::unsigned_integral T1, std::unsigned_integral T2>
		struct common_integral<T1, T2> {
			using type = std::conditional_t<sizeof(T1) >= sizeof(T2), T1, T2>;
		};

		template<std::signed_integral T1, std::signed_integral T2>
		struct common_integral<T1, T2> {
			using type = std::conditional_t<sizeof(T1) >= sizeof(T2), T1, T2>;
		};

		template<std::signed_integral T1, std::unsigned_integral T2>
		struct common_integral<T1, T2> {
			using type = typename common_integral<T1, std::make_signed_t<T2>>::type;
		};

		template<std::unsigned_integral T1, std::signed_integral T2>
		struct common_integral<T1, T2> {
			using type = typename common_integral<std::make_signed_t<T1>, T2>::type;
		};

		template<typename Int, Int start, Int end, typename Acc>
		struct make_integer_sequence;

		template<typename Int, Int start, Int end, Int...acc>
		struct make_integer_sequence<Int, start, end, std::integer_sequence<Int, acc...>> {
			using type = typename make_integer_sequence<Int, start + 1, end, std::integer_sequence<Int, acc..., start>>::type;
		};

		template<typename Int, Int start, Int ...acc>
		struct make_integer_sequence<Int, start, start, std::integer_sequence<Int, acc...>> {
			using type = std::integer_sequence<Int, acc...>;
		};

		template<std::integral auto first, std::integral auto last>
		struct Range {
			using int_type = typename common_integral<decltype(first), decltype(last)>::type;

			static_assert(std::in_range<int_type>(first) && std::in_range<int_type>(last),
			        "default logic for integer type selection is insufficient, please manually specify required type");
		private:
			static constexpr int_type first_typed = first;
			static constexpr int_type last_typed = last;

		public:
			static constexpr int_type min = std::min(first_typed, last_typed);
			static constexpr int_type max = std::max(first_typed, last_typed);
			static constexpr size_t len = static_cast<size_t>(max - min);

			static constexpr size_t distance_to_start(int_type const n) noexcept {
				return static_cast<size_t>(n - min);
			}
		};

		template<typename F, size_t size, typename Int>
		struct vtable {
			using arg_type = Int;
			using return_type = decltype(std::declval<F>().template operator()<arg_type{}>());
			static constexpr bool is_noexcept = noexcept(std::declval<F>().template operator()<arg_type{}>());
			using vtable_member_type = return_type (*)(F &&) noexcept(is_noexcept);

			std::array<vtable_member_type, size> functions;

			static constexpr return_type unreachable_func() noexcept {
				assert(false);
				__builtin_unreachable();
			}

			template<arg_type val>
			static constexpr return_type exec_case_func(F &&f) noexcept(is_noexcept) {
				return std::forward<F>(f).template operator()<val>();
			}

			template<arg_type ...vals>
			[[nodiscard]] static consteval vtable make(std::integer_sequence<arg_type, vals...>) {
				return vtable{.functions = {&exec_case_func<vals>...}};
			}
		};

		template<typename F, size_t size, typename Int, Int start, Int end>
		inline constexpr vtable<F, size, Int> vtable_for = vtable<F, size, Int>::make(typename make_integer_sequence<Int, start, end, std::integer_sequence<Int>>::type{});
	}// namespace detail_switch_cases

	template<typename F, auto ...args>
	concept template_invocable = requires (F f) {
									 f.template operator()<args...>();
								 };

	template<typename F, auto...args>
	concept nothrow_template_invocable = requires (F f) {
											 noexcept(f.template operator()<args...>());
										 };

	/**
   * Generates a switch-case function at compile-time which is evaluated at runtime. The switch is a lambda like: `[&]<auto i>(){ ... }`.
   * It will be instantiated with the values between first and last excluding the maximum value.
   * If any of the <div>cases_function</div>s returns all of them must return. The types of the
   * returned objects must be compatible. This applies also to the default_function.
   * @tparam first first value of the integer range which is switched over. It doesn't matter if first or last is
   * larger. The upper bound is excluded, i.e. [min,max)
   * @tparam last last value of the integer range which is switched over
   * @tparam F type of switch function
   * @tparam D type of default_function
   * @param condition the switch condition
   * @param cases_function the switch function template or lambda. See function description for details.
   * @param default_function the fallback function if `condition` is not in range of [min,max) spanned by first, last
   * @return The value returned from the switch case
   */
	template<std::integral auto first, std::integral auto last, template_invocable<typename detail_switch_cases::Range<first, last>::int_type{}> F, std::invocable D>
	constexpr decltype(auto) switch_cases(typename detail_switch_cases::Range<first, last>::int_type condition, F &&cases_function, D &&default_function)
			noexcept(nothrow_template_invocable<F, typename detail_switch_cases::Range<first, last>::int_type{}> && std::is_nothrow_invocable_v<D>) {

		using range_t = detail_switch_cases::Range<first, last>;
		constexpr auto vtable_size = range_t::len;
		auto const vtable_index = range_t::distance_to_start(condition);

		if (vtable_index >= vtable_size) {
			return std::forward<D>(default_function)();
		}

		auto const &vtable = detail_switch_cases::vtable_for<std::remove_cvref_t<F>,
															 vtable_size,
		        											 typename range_t::int_type,
															 range_t::min,
															 range_t::max>;

		return vtable.functions[vtable_index](std::forward<F>(cases_function));
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
	template<std::integral auto first, std::integral auto last, template_invocable<typename detail_switch_cases::Range<first, last>::int_type{}> F>
	constexpr decltype(auto) switch_cases(typename detail_switch_cases::Range<first, last>::int_type condition, F &&cases_function)
			noexcept(nothrow_template_invocable<F, typename detail_switch_cases::Range<first, last>::int_type{}>) {

		constexpr auto vtable_size = detail_switch_cases::Range<first, last>::len;
		auto const default_func = &detail_switch_cases::vtable<std::remove_cvref_t<F>, vtable_size, decltype(condition)>::unreachable_func;

		return switch_cases<first, last, F>(condition, std::forward<F>(cases_function), default_func);
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
	template<std::integral auto last, template_invocable<decltype(last){}> F, std::invocable D>
	constexpr decltype(auto) switch_cases(decltype(last) const condition, F &&cases_function, D &&default_function)
			noexcept(nothrow_template_invocable<F, decltype(last){}> && std::is_nothrow_invocable_v<D>) {

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
	template<std::integral auto last, template_invocable<decltype(last){}> F>
	constexpr decltype(auto) switch_cases(decltype(last) const condition, F &&cases_function)
			noexcept(nothrow_template_invocable<F, decltype(last){}>) {

		constexpr auto vtable_size = detail_switch_cases::Range<0, last>::len;
		constexpr auto default_func = &detail_switch_cases::vtable<std::remove_cvref_t<F>, vtable_size, decltype(condition)>::unreachable_func;
		return switch_cases<decltype(last){}, last, F>(condition, std::forward<F>(cases_function), default_func);
	}
}// namespace dice::template_library

#endif//HYPERTRIE_SWITCHTEMPLATEFUNCTIONS_HPP
