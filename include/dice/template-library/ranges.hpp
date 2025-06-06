#ifndef DICE_TEMPLATELIBRARY_HPP
#define DICE_TEMPLATELIBRARY_HPP

#include <algorithm>
#include <ranges>

#define DTL_DEFINE_PIPEABLE_RANGE_ALGO(ALGO_NAME)                                  \
	namespace ranges_algo_detail {                                                 \
		template<typename Pred>                                                    \
		struct ALGO_NAME##_fn {                                                    \
			Pred pred;                                                             \
                                                                                   \
			template<typename P>                                                   \
			constexpr explicit ALGO_NAME##_fn(P &&p) : pred(std::forward<P>(p)) {} \
                                                                                   \
			template<std::ranges::input_range R>                                   \
			constexpr bool operator()(R &&r) const {                               \
				return std::ranges::ALGO_NAME(r, pred);                            \
			}                                                                      \
                                                                                   \
			template<typename R>                                                   \
			friend auto operator|(R &&r, const ALGO_NAME##_fn &self)               \
					-> decltype(self(std::forward<R>(r))) {                        \
				return self(std::forward<R>(r));                                   \
			}                                                                      \
		};                                                                         \
	}                                                                              \
                                                                                   \
	template<typename Pred>                                                        \
	constexpr auto ALGO_NAME(Pred &&pred) {                                        \
		return ranges_algo_detail::ALGO_NAME##_fn<std::decay_t<Pred>>(             \
				std::forward<Pred>(pred));                                         \
	}


namespace dice::template_library {
	DTL_DEFINE_PIPEABLE_RANGE_ALGO(all_of)
	DTL_DEFINE_PIPEABLE_RANGE_ALGO(any_of)
	DTL_DEFINE_PIPEABLE_RANGE_ALGO(none_of)
}// namespace dice::template_library
#endif// DICE_TEMPLATELIBRARY_HPP
