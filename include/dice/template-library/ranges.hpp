#ifndef DICE_TEMPLATE_LIBRARY_RANGES_H
#define DICE_TEMPLATE_LIBRARY_RANGES_H


// predicate based terminal algorithm
#define DTL_DEFINE_SINGLE_FUNC_BASED_RANGE_ALGO(algo_name, impl_function, default_func, ...)               \
    namespace ranges_algo_detail {                                                                         \
        template<typename Pred>                                                                            \
        struct algo_name##_pipeline {                                                                      \
        private:                                                                                           \
            Pred pred_;                                                                                    \
                                                                                                           \
        public:                                                                                            \
            explicit constexpr algo_name##_pipeline(Pred pred)                                             \
                : pred_{std::move(pred)} {                                                                 \
            }                                                                                              \
                                                                                                           \
            template<std::ranges::input_range R>                                                           \
            [[nodiscard]] friend constexpr auto operator|(R &&r, algo_name##_pipeline const &self) {       \
                return impl_function(std::ranges::begin(r), std::ranges::end(r), self.pred_);              \
            }                                                                                              \
        };                                                                                                 \
                                                                                                           \
        struct algo_name##_fn {                                                                            \
            template<std::input_iterator I, std::sentinel_for<I> S, typename Pred = default_func>          \
            [[nodiscard]] constexpr auto operator()(I first, S last, Pred pred = {}) const {               \
                return impl_function(std::move(first), std::move(last), std::move(pred));                  \
            }                                                                                              \
                                                                                                           \
            template<std::ranges::input_range R, typename Pred = default_func>                             \
            [[nodiscard]] constexpr auto operator()(R &&range, Pred pred = {}) const {                     \
                return impl_function(std::ranges::begin(range), std::ranges::end(range), std::move(pred)); \
            }                                                                                              \
                                                                                                           \
            template<typename Pred = default_func>                                                         \
            requires (!std::ranges::input_range<Pred> && !std::input_iterator<Pred>)                       \
            [[nodiscard]] constexpr auto operator()(Pred pred = {}) const {                                \
                return algo_name##_pipeline{std::move(pred)};                                              \
            }                                                                                              \
        };                                                                                                 \
    }                                                                                                      \
                                                                                                           \
    __VA_ARGS__ inline constexpr ranges_algo_detail::algo_name##_fn algo_name;

#endif  //DICE_TEMPLATE_LIBRARY_RANGES_H
