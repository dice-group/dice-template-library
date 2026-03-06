#ifndef DICE_TEMPLATE_LIBRARY_OPT_MINMAX_HPP
#define DICE_TEMPLATE_LIBRARY_OPT_MINMAX_HPP

#include <algorithm>
#include <concepts>
#include <optional>
#include <ranges>
#include <type_traits>

namespace dice::template_library
{
    /**
     * Result type for opt_minmax holding both the minimum and maximum.
     * @tparam T the value type
     */
    template <typename T>
    struct opt_minmax_result
    {
        std::optional<T> min;
        std::optional<T> max;
    };

    namespace detail
    {
        /**
         * Type trait to detect std::optional types.
         * @tparam T the type to check
         */
        template <typename T>
        inline constexpr bool is_optional_v = false;

        template <typename T>
        inline constexpr bool is_optional_v<std::optional<T>> = true;

        /**
         * Concept that is satisfied if T is a std::optional.
         */
        template <typename T>
        concept optional_type = is_optional_v<std::remove_cvref_t<T>>;


        // Normalizes T to std::optional<T>. Traps nested optionals immediately.
        template <typename T>
        struct normalize_opt
        {
            static_assert(!is_optional_v<T>, "Nested std::optional is not supported.");
            using type = std::optional<T>;
        };

        // If it's already an optional, unpack it to check for nesting, then return it.
        template <typename T>
        struct normalize_opt<std::optional<T>>
        {
            static_assert(!is_optional_v<T>, "Nested std::optional is not supported.");
            using type = std::optional<T>;
        };

        // Leave nullopt_t alone so std::common_type_t can process it properly
        template <>
        struct normalize_opt<std::nullopt_t>
        {
            using type = std::nullopt_t;
        };

        template <typename T>
        using normalize_opt_t = typename normalize_opt<std::remove_cvref_t<T>>::type;

        /**
         * Helper type allowing 0-argument opt_minmax to convert to any opt_minmax_result<T>.
         */
        struct empty_opt_minmax_result
        {
            template <typename T>
            constexpr operator opt_minmax_result<T>() const noexcept
            {
                return opt_minmax_result<T>{std::nullopt, std::nullopt};
            }
        };
    } // namespace detail

    /**
     * Returns the minimum of the given arguments, treating std::nullopt as "no value".
     * With 0 arguments, returns std::nullopt.
     * @param args values or optionals to compare
     * @return std::optional containing the minimum, or std::nullopt if all are empty
     */
    [[nodiscard]] constexpr std::nullopt_t opt_min() noexcept
    {
        return std::nullopt;
    }

    /**
     * Returns the minimum of the given arguments, treating std::nullopt as "no value".
     * With 0 arguments and an explicit type, returns an empty std::optional<T>.
     * @tparam T the value type
     * @return empty std::optional<T>
     */
    template <typename T>
    [[nodiscard]] constexpr std::optional<T> opt_min() noexcept
    {
        return std::nullopt;
    }

    /**
     * Returns the maximum of the given arguments, treating std::nullopt as "no value".
     * With 0 arguments, returns std::nullopt.
     * @param args values or optionals to compare
     * @return std::optional containing the maximum, or std::nullopt if all are empty
     */
    [[nodiscard]] constexpr std::nullopt_t opt_max() noexcept
    {
        return std::nullopt;
    }

    /**
     * Returns the maximum of the given arguments, treating std::nullopt as "no value".
     * With 0 arguments and an explicit type, returns an empty std::optional<T>.
     * @tparam T the value type
     * @return empty std::optional<T>
     */
    template <typename T>
    [[nodiscard]] constexpr std::optional<T> opt_max() noexcept
    {
        return std::nullopt;
    }

    /**
     * Returns both the minimum and maximum of the given arguments.
     * With 0 arguments, returns an empty result convertible to any opt_minmax_result<T>.
     * @param args values or optionals to compare
     * @return opt_minmax_result containing min and max, or nullopt for both if all are empty
     */
    [[nodiscard]] constexpr detail::empty_opt_minmax_result opt_minmax() noexcept
    {
        return {};
    }

    /**
     * Returns both the minimum and maximum of the given arguments.
     * With 0 arguments and an explicit type, returns an empty opt_minmax_result<T>.
     * @tparam T the value type
     * @return opt_minmax_result with both fields set to std::nullopt
     */
    template <typename T>
    [[nodiscard]] constexpr opt_minmax_result<T> opt_minmax() noexcept
    {
        return {std::nullopt, std::nullopt};
    }

    /// @copydoc opt_min()
    template <typename... Args>
        requires (sizeof...(Args) > 0) && requires { typename std::common_type_t<detail::normalize_opt_t<Args>...>; }
    [[nodiscard]] constexpr auto opt_min(Args const&... args)
    {
        using CommonT = std::remove_cvref_t<std::common_type_t<detail::normalize_opt_t<Args>...>>;

        if constexpr (std::same_as<CommonT, std::nullopt_t>)
        {
            return std::nullopt;
        }
        else
        {
            CommonT result = std::nullopt;
            auto update = [&result](auto const& raw_val)
            {
                CommonT val = raw_val;
                if (val && (!result || *val < *result))
                {
                    result = val;
                }
            };
            (update(args), ...);
            return result;
        }
    }

    /// @copydoc opt_max()
    template <typename... Args>
        requires (sizeof...(Args) > 0) && requires { typename std::common_type_t<detail::normalize_opt_t<Args>...>; }
    [[nodiscard]] constexpr auto opt_max(Args const&... args)
    {
        using CommonT = std::remove_cvref_t<std::common_type_t<detail::normalize_opt_t<Args>...>>;

        if constexpr (std::same_as<CommonT, std::nullopt_t>)
        {
            return std::nullopt;
        }
        else
        {
            CommonT result = std::nullopt;
            auto update = [&result](auto const& raw_val)
            {
                CommonT val = raw_val;
                if (val && (!result || *result < *val))
                {
                    result = val;
                }
            };
            (update(args), ...);
            return result;
        }
    }

    /// @copydoc opt_minmax()
    template <typename... Args>
        requires (sizeof...(Args) > 0) && requires { typename std::common_type_t<detail::normalize_opt_t<Args>...>; }
    [[nodiscard]] constexpr auto opt_minmax(Args const&... args)
    {
        using CommonT = std::remove_cvref_t<std::common_type_t<detail::normalize_opt_t<Args>...>>;

        if constexpr (std::same_as<CommonT, std::nullopt_t>)
        {
            return detail::empty_opt_minmax_result{};
        }
        else
        {
            using T = typename CommonT::value_type;
            opt_minmax_result<T> result{std::nullopt, std::nullopt};

            auto update = [&result](auto const& raw_val)
            {
                CommonT val = raw_val;
                if (val)
                {
                    if (!result.min || *val < *result.min)
                    {
                        result.min = val;
                    }
                    if (!result.max || *result.max < *val)
                    {
                        result.max = val;
                    }
                }
            };
            (update(args), ...);
            return result;
        }
    }

    /**
     * Returns the minimum element from a range, treating std::nullopt elements as "no value".
     * @tparam R an input range type
     * @param r the range to search
     * @return std::optional containing the minimum, or std::nullopt if the range is empty or all elements are nullopt
     */
    template <std::ranges::input_range R>
        requires requires { typename detail::normalize_opt_t<std::ranges::range_value_t<R>>; }
    [[nodiscard]] constexpr auto opt_min_range(R&& r)
    {
        using CommonT = detail::normalize_opt_t<std::ranges::range_value_t<R>>;

        if constexpr (std::same_as<CommonT, std::nullopt_t>)
        {
            return std::nullopt;
        }
        else
        {
            CommonT result = std::nullopt;
            for (auto const& raw_val : r)
            {
                CommonT val = raw_val;
                if (val && (!result || *val < *result))
                {
                    result = val;
                }
            }
            return result;
        }
    }

    /**
     * Returns the maximum element from a range, treating std::nullopt elements as "no value".
     * @tparam R an input range type
     * @param r the range to search
     * @return std::optional containing the maximum, or std::nullopt if the range is empty or all elements are nullopt
     */
    template <std::ranges::input_range R>
        requires requires { typename detail::normalize_opt_t<std::ranges::range_value_t<R>>; }
    [[nodiscard]] constexpr auto opt_max_range(R&& r)
    {
        using CommonT = detail::normalize_opt_t<std::ranges::range_value_t<R>>;

        if constexpr (std::same_as<CommonT, std::nullopt_t>)
        {
            return std::nullopt;
        }
        else
        {
            CommonT result = std::nullopt;
            for (auto const& raw_val : r)
            {
                CommonT val = raw_val;
                if (val && (!result || *result < *val))
                {
                    result = val;
                }
            }
            return result;
        }
    }

    /**
     * Returns both the minimum and maximum elements from a range.
     * @tparam R an input range type
     * @param r the range to search
     * @return opt_minmax_result containing min and max, or nullopt for both if the range is empty or all elements are nullopt
     */
    template <std::ranges::input_range R>
        requires requires { typename detail::normalize_opt_t<std::ranges::range_value_t<R>>; }
    [[nodiscard]] constexpr auto opt_minmax_range(R&& r)
    {
        using CommonT = detail::normalize_opt_t<std::ranges::range_value_t<R>>;

        if constexpr (std::same_as<CommonT, std::nullopt_t>)
        {
            return detail::empty_opt_minmax_result{};
        }
        else
        {
            using T = typename CommonT::value_type;
            opt_minmax_result<T> result{std::nullopt, std::nullopt};

            for (auto const& raw_val : r)
            {
                CommonT val = raw_val;
                if (val)
                {
                    if (!result.min || *val < *result.min)
                    {
                        result.min = val;
                    }
                    if (!result.max || *result.max < *val)
                    {
                        result.max = val;
                    }
                }
            }
            return result;
        }
    }
} // namespace dice::template_library

#endif // DICE_TEMPLATE_LIBRARY_OPT_MINMAX_HPP
