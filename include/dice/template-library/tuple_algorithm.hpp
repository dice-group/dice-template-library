#ifndef DICE_TEMPLATE_LIBRARY_TUPLEALGORITHM_HPP
#define DICE_TEMPLATE_LIBRARY_TUPLEALGORITHM_HPP

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dice::template_library {
	namespace tuple_algo_detail {
		template<typename Tuple, typename Acc, typename FoldF, size_t... Ixs>
		constexpr Acc tuple_type_fold_impl(std::index_sequence<Ixs...>, Acc init, FoldF f) noexcept {
			((init = f.template operator()<std::tuple_element_t<Ixs, Tuple>>(std::move(init))), ...);
			return init;
		}

		template<typename Tuple, typename Acc, typename FoldF, size_t... Ixs>
		constexpr Acc tuple_fold_impl(std::index_sequence<Ixs...>, Tuple const &tuple, Acc init, FoldF f) noexcept {
			((init = f(std::move(init), std::get<Ixs>(tuple))), ...);
			return init;
		}

		template<typename Tuple, typename F, size_t... Ixs>
		constexpr void tuple_for_each_impl(std::index_sequence<Ixs...>, Tuple &&tuple, F f) {
			(f(std::get<Ixs>(std::forward<Tuple>(tuple))), ...);
		}

		template<typename Tuple, typename F, size_t... Ixs>
		constexpr void tuple_type_for_each_impl(std::index_sequence<Ixs...>, F f) {
			(f.template operator()<std::tuple_element_t<Ixs, Tuple>>(), ...);
		}
	} // namespace tuple_algo_detail

	/**
	 * Folds over all elements of the given tuple.
	 * I.e. applies accumulator = f(accumulator, tuple-element) for each element in the tuple
	 *
	 * @param tuple tuple to fold over
	 * @param init initial value for the accumulator
	 * @param f binop for accumulator, tuple-element
	 * @return the final value of the accumulator
	 */
	template<typename Tuple, typename Acc, typename FoldF>
	constexpr Acc tuple_fold(Tuple const &tuple, Acc &&init, FoldF &&f) noexcept {
		return tuple_algo_detail::tuple_fold_impl(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Tuple>>>{}, tuple, std::forward<Acc>(init), std::forward<FoldF>(f));
	}

	/**
	 * Folds over all types of the given tuple.
	 * I.e. applies accumulator = f.template operator()<tuple-element-type>(accumulator) for each type in the tuple
	 *
	 * @tparam Tuple type of the tuple to fold over
	 * @param init initial value for the accumulator
	 * @param f partially templated binop for tuple-type and accumulator
	 * @return the final value of the accumulator
	 */
	template<typename Tuple, typename Acc, typename FoldF>
	constexpr Acc tuple_type_fold(Acc &&init, FoldF &&f) noexcept {
		return tuple_algo_detail::tuple_type_fold_impl<Tuple>(std::make_index_sequence<std::tuple_size_v<Tuple>>{}, std::forward<Acc>(init), std::forward<FoldF>(f));
	}

	/**
	 * Applies a function to all elements of a tuple
	 *
	 * @param tuple tuple with elements to apply function to
	 * @param f unop to apply to each element
	 */
	template<typename Tuple, typename F>
	constexpr void tuple_for_each(Tuple &&tuple, F &&f) {
		return tuple_algo_detail::tuple_for_each_impl(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Tuple>>>{}, std::forward<Tuple>(tuple), std::forward<F>(f));
	}

	/**
	 * Applies a function to all types of a tuple
	 *
	 * @param tuple tuple with elements to apply function to
	 * @param f template unop to apply to each type (as f.template operator()<tuple-element-type>())
	 */
	template<typename Tuple, typename F>
	constexpr void tuple_type_for_each(F &&f) {
		return tuple_algo_detail::tuple_type_for_each_impl<Tuple>(std::make_index_sequence<std::tuple_size_v<Tuple>>{}, std::forward<F>(f));
	}
  
} // namespace dice::template_library

#endif // DICE_TEMPLATE_LIBRARY_TUPLEALGORITHM_HPP
