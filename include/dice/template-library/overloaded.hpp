#ifndef DICE_TEMPLATE_LIBRARY_OVERLOADED_HPP
#define DICE_TEMPLATE_LIBRARY_OVERLOADED_HPP

namespace dice::template_library {

	/**
	 * A utility struct that creates an overload set on-the-fly out of many functions (Fs...).
	 * This is primarily useful for things like std::visit where you may want to call a different
	 * function for each type in the std::variant.
	 *
	 * See examples/examples_overloaded.cpp for a usecase example
	 */
	template<typename ...Fs>
	struct overloaded : Fs... {
		using Fs::operator()...;
	};

	template<typename ...Fs>
	overloaded(Fs...) -> overloaded<Fs...>;

} // namespace dice::template_library

#endif // DICE_TEMPLATE_LIBRARY_OVERLOADED_HPP
