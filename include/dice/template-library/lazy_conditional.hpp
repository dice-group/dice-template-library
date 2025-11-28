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

} // namespace dice::template_library

#endif // DICE_TEMPLATE_LIBRARY_LAZY_CONDITIONAL_HPP