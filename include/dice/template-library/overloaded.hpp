#ifndef DICE_TEMPLATE_LIBRARY_OVERLOADED_HPP
#define DICE_TEMPLATE_LIBRARY_OVERLOADED_HPP

#include <utility>

namespace dice::template_library {

	/**
	 * A utility struct that creates an overload set on-the-fly out of many functions (Fs...).
	 * This is primarily useful for things like std::visit where you may want to call a different
	 * function for each type in the std::variant.
	 *
	 * See example on https://en.cppreference.com/w/cpp/utility/variant/visit
	 */
	template<typename ...Fs>
	struct overloaded : Fs... {
		using Fs::operator()...;
	};

	template<typename ...Fs>
	overloaded(Fs...) -> overloaded<Fs...>;

	template<typename Variant, typename ...Fs>
	decltype(auto) match(Variant &&variant, Fs &&...visitors) {
		return visit(overloaded{std::forward<Fs>(visitors)...}, std::forward<Variant>(variant));
	}

} // namespace dice::template_library

#endif // DICE_TEMPLATE_LIBRARY_OVERLOADED_HPP
