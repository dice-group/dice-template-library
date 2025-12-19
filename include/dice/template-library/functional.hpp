#ifndef DICE_TEMPLATELIBRARY_FUNCTIONAL_HPP
#define DICE_TEMPLATELIBRARY_FUNCTIONAL_HPP

#include <dice/template-library/type_traits.hpp>

#include <functional>
#include <utility>
#include <type_traits>

namespace dice::template_library {

	namespace detail_bind_front {
		template<typename T, typename U, typename X = dice::template_library::copy_const_t<std::remove_reference_t<T>, U>>
		struct copy_value_category : std::conditional<std::is_lvalue_reference_v<T&&>, X&, X&&> {
		};

		template<typename T, typename U>
		struct type_forward_like : copy_value_category<T, std::remove_reference_t<U>> {
		};

		/**
		 * From https://en.cppreference.com/w/cpp/utility/functional/bind_front.html
		 */
		template<typename T, typename U>
		using type_forward_like_t = typename type_forward_like<T, U>::type;
	} // namespace detail_bind_front

	/**
	 * Implementation of std::bind_front with constexpr function.
	 * See https://en.cppreference.com/w/cpp/utility/functional/bind_front.html
	 *
	 * @note this implementation mainly exists because the version with constexpr function is only available since C++26
	 */
	template<auto func, typename... BindArgs>
	[[nodiscard]] constexpr auto bind_front(BindArgs &&...bind_args) {
		using func_t = decltype(func);
		static_assert(!(std::is_pointer_v<func_t> || std::is_member_pointer_v<func_t>) || func != nullptr,
					  "Cannot bind arguments to nullptr");

		return [...bound_args(std::forward<BindArgs>(bind_args))]<typename Self, typename ...RestArgs>([[maybe_unused]] this Self &&self, RestArgs &&...rest_args)
				noexcept(std::is_nothrow_invocable_v<func_t, detail_bind_front::type_forward_like_t<Self, std::decay_t<BindArgs>>..., decltype(std::forward<RestArgs>(rest_args))...>) -> decltype(auto) {
			return std::invoke(func, dice::template_library::forward_like<Self>(bound_args)..., std::forward<RestArgs>(rest_args)...);
		};
	}

}// namespace dice::template_library

#endif// DICE_TEMPLATELIBRARY_FUNCTIONAL_HPP
