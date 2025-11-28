#ifndef DICE_TEMPLATE_LIBRARY_LAZY_CONDITIONAL_HPP
#define DICE_TEMPLATE_LIBRARY_LAZY_CONDITIONAL_HPP

namespace dice::template_library {

	/**
	 * Lazy conditional type selection.
	 *
	 * Unlike std::conditional which eagerly evaluates both branches, lazy_conditional only
	 * accesses the ::type member of the selected branch. This is crucial when one branch
	 * would cause a compilation error if instantiated.
	 *
	 * @tparam enabled The condition to evaluate
	 * @tparam TrueBranchProvider Type provider for the true branch (must have a ::type member)
	 * @tparam FalseBranchProvider Type provider for the false branch (must have a ::type member)
	 *
	 * Example usage:
	 * @code
	 * template<typename T>
	 * struct get_value_type {
	 *     using type = typename T::value_type;
	 * };
	 *
	 * template<typename T>
	 * struct identity {
	 *     using type = T;
	 * };
	 *
	 * template<typename T>
	 * using extract_value_type_t = lazy_conditional_t<
	 *     std::is_class_v<T>,
	 *     get_value_type<T>,  // Only accessed if T is a class
	 *     identity<T>
	 * >;
	 *
	 * // This works even though int::value_type would be invalid
	 * using result = extract_value_type_t<int>; // result is int
	 * @endcode
	 */
	template<bool enabled, typename TrueBranchProvider, typename FalseBranchProvider>
	struct lazy_conditional {
		using type = typename TrueBranchProvider::type;
	};

	/**
	 * Specialization for when the condition is false.
	 */
	template<typename TrueBranchProvider, typename FalseBranchProvider>
	struct lazy_conditional<false, TrueBranchProvider, FalseBranchProvider> {
		using type = typename FalseBranchProvider::type;
	};

	/**
	 * Helper alias template for lazy_conditional.
	 * @see lazy_conditional
	 */
	template<bool enabled, typename T, typename F>
	using lazy_conditional_t = typename lazy_conditional<enabled, T, F>::type;

	/**
	 * Case helper for lazy_switch.
	 * Wraps a condition and a type provider together.
	 *
	 * @tparam Cond The compile-time boolean condition
	 * @tparam Provider Type provider that has a ::type member
	 */
	template<bool Cond, typename Provider>
	struct case_ {
		static constexpr bool condition = Cond;
		using provider = Provider;
	};

	/**
	 * Lazy switch-case type selection.
	 *
	 * Evaluates cases in order and selects the first case where the condition is true.
	 * Only the selected provider's ::type member is accessed, preventing compilation
	 * errors in non-selected branches.
	 *
	 * @tparam DefaultProvider Type provider for the default case (used if no case matches)
	 * @tparam Cases Variadic list of case_<bool, Provider> instances
	 *
	 * Example usage:
	 * @code
	 * template<typename T>
	 * struct integral_handler {
	 *     using type = int;
	 * };
	 *
	 * template<typename T>
	 * struct float_handler {
	 *     using type = double;
	 * };
	 *
	 * template<typename T>
	 * struct default_handler {
	 *     static_assert(sizeof(T) == 0, "Unsupported type");
	 *     using type = void;
	 * };
	 *
	 * template<typename T>
	 * using select_handler = lazy_switch_t<
	 *     default_handler<T>,
	 *     case_<std::is_integral_v<T>, integral_handler<T>>,
	 *     case_<std::is_floating_point_v<T>, float_handler<T>>
	 * >;
	 *
	 * using result = select_handler<int>; // result is int
	 * // default_handler is never instantiated, so its static_assert doesn't fire
	 * @endcode
	 */
	template<typename DefaultProvider, typename... Cases>
	struct lazy_switch_default;

	// Base case: no cases left, use default
	template<typename DefaultProvider>
	struct lazy_switch_default<DefaultProvider> {
		using type = typename DefaultProvider::type;
	};

	// Recursive case: check first case, recurse if false
	template<typename DefaultProvider, bool Cond, typename Provider, typename... Rest>
	struct lazy_switch_default<DefaultProvider, case_<Cond, Provider>, Rest...> {
		using type = lazy_conditional_t<
				Cond,
				Provider,
				lazy_switch_default<DefaultProvider, Rest...>>;
	};

	/**
	 * Helper alias template for lazy_switch.
	 * @see lazy_switch
	 */
	template<typename DefaultProvider, typename... Cases>
	using lazy_switch_default_t = typename lazy_switch_default<DefaultProvider, Cases...>::type;

	/**
	 * Lazy switch without default - only valid if at least one case matches.
	 * If no case matches, this will cause a compilation error.
	 *
	 * @tparam Cases Variadic list of case_<bool, Provider> instances
	 */
	template<typename... Cases>
	struct lazy_switch;

	// Recursive case: check first case, recurse if false
	template<bool Cond, typename Provider, typename... Rest>
	struct lazy_switch<case_<Cond, Provider>, Rest...> {
		using type = lazy_conditional_t<
				Cond,
				Provider,
				lazy_switch<Rest...>>;
	};

	/**
	 * Helper alias template for lazy_switch without default.
	 * Evaluates cases in order and selects the first matching case.
	 * If no case matches, compilation will fail.
	 *
	 * @see lazy_switch_no_default
	 */
	template<typename... Cases>
	using lazy_switch_t = lazy_switch<Cases...>::type;

} // namespace dice::template_library

#endif // DICE_TEMPLATE_LIBRARY_LAZY_CONDITIONAL_HPP