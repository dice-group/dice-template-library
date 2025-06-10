#ifndef DICE_TEMPLATE_LIBRARY_MEMFN_HPP
#define DICE_TEMPLATE_LIBRARY_MEMFN_HPP

#include <utility>

/**
 * @brief Creates a lambda that calls a member function on the current object. Arguments, if any are required, are forwarded.
 *
 * @param member_func The name of the member function to call. They are captured from the context.
 * @param ...args A list of arguments to pass to the call. These bind to the arguments at the front.
 * @param ...runtime_args Some functions, like range_adaptors, pass in an element. This is used to capture that. Binds to arguments after ...args
 *
 */
#define DICE_MEMFN(member_func, ...)                                                                     \
	([&, this]<typename... RuntimeArgs>(RuntimeArgs &&...runtime_args) -> decltype(auto) {               \
		return this->member_func(__VA_ARGS__ __VA_OPT__(, ) std::forward<RuntimeArgs>(runtime_args)...); \
	})
#endif// DICE_TEMPLATE_LIBRARY_MEMFN_HPP