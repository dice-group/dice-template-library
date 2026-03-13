#ifndef DICE_TEMPLATE_LIBRARY_OPT_MINMAX_HPP
#define DICE_TEMPLATE_LIBRARY_OPT_MINMAX_HPP

#include <algorithm>
#include <functional>
#include <optional>
#include <ranges>

#include <dice/template-library/ranges.hpp>

namespace dice::template_library
{
    /**
     * Result type for opt_minmax holding both the minimum and maximum.
     * @tparam T the value type
     */
    template<typename T>
    struct opt_minmax_result {
        T min;
        T max;
    };

    namespace detail_opt_min_max {
        template<typename T>
        struct into_optional {
            using type = std::optional<T>;
        };

        template<typename T>
        struct into_optional<std::optional<T>> {
            using type = std::optional<T>;
        };

        template<typename T>
        struct into_minmax_result {
            using type = std::optional<opt_minmax_result<T>>;
        };

        template<typename T>
        struct into_minmax_result<std::optional<T>> {
            using type = std::optional<opt_minmax_result<T>>;
        };

        template<typename T>
        [[nodiscard]] constexpr bool has_value(std::optional<T> const &val) noexcept {
            return val.has_value();
        }

        template<typename T>
        [[nodiscard]] constexpr bool has_value([[maybe_unused]] T const &val) noexcept {
            return true;
        }

        template<typename T>
        [[nodiscard]] constexpr T const &deref(std::optional<T> const &val) noexcept {
            return *val;
        }

        template<typename T>
        [[nodiscard]] constexpr T const &deref(T const &val) noexcept {
            return val;
        }

        template<std::input_iterator I, std::sentinel_for<I> S, typename Cmp>
        [[nodiscard]] constexpr into_optional<std::iter_value_t<I>>::type opt_min_element_impl(I first, S last, Cmp cmp) {
            using T = std::iter_value_t<I>;

            typename into_optional<T>::type ret;
            for (; first != last; ++first) {
                if (auto val = *first; has_value(val)) {
                    if (ret.has_value()) {
                        if (std::invoke(cmp, deref(val), *ret)) {
                            ret = deref(val);
                        }
                    } else {
                        ret = deref(val);
                    }
                }
            }

            return ret;
        }

        template<std::input_iterator I, std::sentinel_for<I> S, typename Cmp>
        [[nodiscard]] constexpr into_optional<std::iter_value_t<I>>::type opt_max_element_impl(I first, S last, Cmp cmp) {
            using T = std::iter_value_t<I>;

            typename into_optional<T>::type ret;
            for (; first != last; ++first) {
                if (auto val = *first; has_value(val)) {
                    if (ret.has_value()) {
                        if (std::invoke(cmp, *ret, deref(val))) {
                            ret = deref(val);
                        }
                    } else {
                        ret = deref(val);
                    }
                }
            }

            return ret;
        }

        template<std::input_iterator I, std::sentinel_for<I> S, typename Cmp>
        [[nodiscard]] constexpr into_minmax_result<std::iter_value_t<I>>::type opt_minmax_element_impl(I first, S last, Cmp cmp) {
            using T = std::iter_value_t<I>;

            typename into_minmax_result<T>::type ret = std::nullopt;
            for (; first != last; ++first) {
                if (auto val = *first; has_value(val)) {
                    if (ret.has_value()) {
                        if (std::invoke(cmp, deref(val), ret->min)) {
                            ret->min = deref(val);
                        }
                        if (std::invoke(cmp, ret->max, deref(val))) {
                            ret->max = deref(val);
                        }
                    } else {
                        ret.emplace(deref(val), deref(val));
                    }
                }
            }

            return ret;
        }

    } // namespace detail_opt_min_max

    /**
     * Returns the minimum element from a range, treating std::nullopt elements as "no value".
     * @param range or first+last the range to search
     * @þaram cmp comparator
     * @return std::optional containing the minimum, or std::nullopt if the range is empty or all elements are nullopt
     */
    DTL_DEFINE_SINGLE_FUNC_BASED_RANGE_ALGO(opt_min_element, detail_opt_min_max::opt_min_element_impl, std::ranges::less);

    /**
     * @return the minimum of the given arguments, treating std::nullopt as "no value".
     */
    template<typename T, typename Cmp = std::less<T>>
    [[nodiscard]] constexpr std::optional<T> opt_min(std::optional<T> const &a, std::optional<T> const &b, Cmp cmp = {}) {
        if (!a.has_value()) {
            return b;
        }
        if (!b.has_value()) {
            return a;
        }

        return std::min(*a, *b, cmp);
    }

    /**
     * @return the minimum of the given elements in the initializer list, treating std::nullopt as "no value".
     */
    template<typename T, typename Cmp = std::less<T>>
    [[nodiscard]] constexpr std::optional<T> opt_min(std::initializer_list<std::optional<T>> ilist, Cmp cmp = {}) {
        return opt_min_element(ilist, cmp);
    }


    /**
     * Returns the maximum element from a range using a custom comparator.
     * The comparator should return true if the first argument is less than the second.
     * @param range or first+last the range to search
     * @param cmp comparator defining the less-than relation
     * @return std::optional containing the maximum, or std::nullopt if the range is empty or all elements are nullopt
     */
    DTL_DEFINE_SINGLE_FUNC_BASED_RANGE_ALGO(opt_max_element, detail_opt_min_max::opt_max_element_impl, std::ranges::less);


    /**
     * @return the maximum of the given arguments, treating std::nullopt as "no value".
     */
    template<typename T, typename Cmp = std::less<T>>
    [[nodiscard]] constexpr std::optional<T> opt_max(std::optional<T> const &a, std::optional<T> const &b, Cmp cmp = {}) {
        if (!a.has_value()) {
            return b;
        }
        if (!b.has_value()) {
            return a;
        }

        return std::max(*a, *b, cmp);
    }

    /**
     * @return the maximum of the given elements in the initializer list, treating std::nullopt as "no value".
     */
    template<typename T, typename Cmp = std::less<T>>
    [[nodiscard]] constexpr std::optional<T> opt_max(std::initializer_list<std::optional<T>> ilist, Cmp cmp = {}) {
        return opt_max_element(ilist, cmp);
    }


    /**
     * Returns both the minimum and maximum elements from a range.
     * @param range or first+last the range to search
     * @param cmp comparator
     * @return opt_minmax_result containing min and max, or nullopt if the range is empty or all elements are nullopt
     */
    DTL_DEFINE_SINGLE_FUNC_BASED_RANGE_ALGO(opt_minmax_element, detail_opt_min_max::opt_minmax_element_impl, std::ranges::less);


    /**
     * Returns both the minimum and maximum elements of the given initializer list.
     *
     * @param ilist first value
     * @param cmp comparator
     * @return opt_minmax_result containing min and max, or nullopt if all are empty
     */
    template<typename T, typename Cmp = std::less<T>>
    [[nodiscard]] constexpr std::optional<opt_minmax_result<T>> opt_minmax(std::initializer_list<std::optional<T>> ilist, Cmp cmp = {}) {
        return opt_minmax_element(ilist, cmp);
    }

    /**
     * Returns both the minimum and maximum of the given arguments.
     *
     * @param a first value
     * @param b second value
     * @param cmp comparator
     * @return opt_minmax_result containing min and max, or nullopt if all are empty
     */
    template<typename T, typename Cmp = std::less<T>>
    [[nodiscard]] constexpr std::optional<opt_minmax_result<T>> opt_minmax(std::optional<T> const &a, std::optional<T> const &b, Cmp cmp = {}) {
        return opt_minmax({a, b}, cmp);
    }

} // namespace dice::template_library

#endif // DICE_TEMPLATE_LIBRARY_OPT_MINMAX_HPP
