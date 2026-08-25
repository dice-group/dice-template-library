#ifndef DICE_TEMPLATELIBRARY_TRYTRAITS_HPP
#define DICE_TEMPLATELIBRARY_TRYTRAITS_HPP

#include <dice/template-library/control_flow.hpp>
#include <dice/template-library/overloaded.hpp>
#include <dice/template-library/type_traits.hpp>

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

#if __has_include(<expected>)
#include <expected>
#endif // __has_include(<expected>)

namespace dice::template_library {

    template<typename T>
    struct try_traits;

    namespace detail_try_traits {
        /**
         * just a type to check rebind to avoid using
         * any fixed type that could have been in the try_result before rebind
         */
        struct rebind_probe {};

        template<typename From, typename To>
        concept generalized_convertible_to = (std::is_void_v<From> && std::is_default_constructible_v<To>) || (!std::is_void_v<From> && std::is_convertible_v<From, To>);

        static_assert(generalized_convertible_to<int, control_flow<int, double>>);
        static_assert(generalized_convertible_to<void, control_flow<int, void>>);
    } // detail_try_traits

    /**
     * A type that can be used in try_* algorithms.
     */
    template<typename T>
    concept try_result = requires (T self) {
        /**
         * The non-error value produced by unwrapping an instance of self.
         * e.g. for optional<T> this is T
         */
        typename try_traits<T>::output_type;

        /**
         * All possible values of self which are **not** represented in output_type.
         */
        typename try_traits<T>::residual_type;

        /**
         * Rebind the output_type of self.
         */
        typename try_traits<T>::template rebind_output<detail_try_traits::rebind_probe>;

        /**
         * output_type can be converted to self
         */
        requires detail_try_traits::generalized_convertible_to<typename try_traits<T>::output_type, T>;

        /**
         * Used to decide if a value is produced (cfcontinue) or a value should be propagated to the caller (cfbreak).
         */
        { try_traits<T>::branch(std::move(self)) } -> std::same_as<control_flow<typename try_traits<T>::residual_type, typename try_traits<T>::output_type>>;

        /**
         * rebinding the output must keep the residual channel intact, i.e. residuals of T must remain usable as residuals of the rebound type
         */
        requires std::same_as<typename try_traits<typename try_traits<T>::template rebind_output<detail_try_traits::rebind_probe>>::output_type, detail_try_traits::rebind_probe>;
        requires std::convertible_to<typename try_traits<T>::residual_type, typename try_traits<T>::template rebind_output<detail_try_traits::rebind_probe>>;
    };


    /**
     * @note constrained to control_flows that have both arms, because a control_flow that cannot continue has
     *      no output_type and a control_flow that cannot break has no (representable) residual_type.
     *      Without the constraint, instantiating try_traits for those would be a hard error instead of
     *      making them simply not satisfy try_result.
     */
    template<typename B, typename C>
    struct try_traits<control_flow<B, C>> {
        using self_type = control_flow<B, C>;
        using output_type = C;
        using residual_type = cfbreak<B>;

        template<typename Out>
        using rebind_output = control_flow<B, Out>;

        static constexpr control_flow<residual_type, output_type> branch(self_type self) {
            return match(std::move(self),
                [](cfcontinue<C> &&cont) -> control_flow<residual_type, output_type> {
                    return std::move(cont);
                },
                [](cfbreak<B> &&brk) -> control_flow<residual_type, output_type> {
                    return control_flow<residual_type, output_type>{in_place_break, std::move(brk)};
                }
            );
        }
    };
    static_assert(try_result<control_flow<int, int>>);

    template<typename T>
    struct try_traits<std::optional<T>> {
        using self_type = std::optional<T>;
        using output_type = T;
        using residual_type = std::nullopt_t;

        template<typename Out>
        using rebind_output = std::optional<Out>;

        static constexpr control_flow<residual_type, output_type> branch(self_type self) {
            if (self.has_value()) {
                return control_flow<residual_type, output_type>{in_place_continue, std::move(*self)};
            }

            return control_flow<residual_type, output_type>{in_place_break, std::nullopt};
        }
    };
    static_assert(try_result<std::optional<int>>);


#if __cpp_lib_expected >= 202202L
    template<typename T, typename E>
    struct try_traits<std::expected<T, E>> {
        using self_type = std::expected<T, E>;
        using output_type = T;
        using residual_type = std::unexpected<E>;

        template<typename Out>
        using rebind_output = std::expected<Out, E>;

        static constexpr control_flow<residual_type, output_type> branch(self_type self) {
            if (self.has_value()) {
                if constexpr (std::is_void_v<T>) {
                    return control_flow<residual_type, output_type>{in_place_continue};
                } else {
                    return control_flow<residual_type, output_type>{in_place_continue, std::move(*self)};
                }
            }

            return control_flow<residual_type, output_type>{in_place_break, std::unexpected{std::move(self.error())}};
        }
    };
    static_assert(try_result<std::expected<int, int>>);
#endif // __cpp_lib_expected >= 202202L

} // namespace dice::template_library

#if defined(__GNUC__) || defined(__clang__)

/**
 * Like rust's ? operator.
 * If the expression is an error/break, returns the error from the current function.
 * If the expression is not an error/break resolves to the ok/continue value.
 *
 * @param expr
 *
 * @par Example
 * @code
 * std::expected<int, int> fallible_function();
 *
 * std::expected<int, int> func() {
 *     int x = DICE_TRY(fallible_function());
 *     return x + 1;
 * }
 * @endcode
 *
 * Requires compiler extensions to work. But good news, they have been in the compilers
 * since basically the beginning.
 */
#define DICE_TRY(...)                                                                                 \
    ({                                                                                                \
        decltype(auto) _dice_try_expr = (__VA_ARGS__);                                                \
        using _dice_try_expr_type = std::remove_cvref_t<decltype(_dice_try_expr)>;                    \
        static_assert(::dice::template_library::try_result<_dice_try_expr_type>,                      \
                      "The expression passed to DICE_TRY must be a try_result (e.g. std::optional)"); \
        using _dice_try_traits = ::dice::template_library::try_traits<_dice_try_expr_type>;           \
                                                                                                      \
        auto _dice_try_res = _dice_try_traits::branch(DICE_MOVE_IF_VALUE(_dice_try_expr));            \
        if (_dice_try_res.is_break()) {                                                               \
            return std::move(_dice_try_res).get_break();                                              \
        }                                                                                             \
                                                                                                      \
        std::move(_dice_try_res).get_continue();                                                      \
    })

#endif // defined(__GNUC__) || defined(__clang__)

#endif // DICE_TEMPLATELIBRARY_TRYTRAITS_HPP
