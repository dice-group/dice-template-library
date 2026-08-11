#ifndef DICE_TEMPLATELIBRARY_TRYTRAITS_HPP
#define DICE_TEMPLATELIBRARY_TRYTRAITS_HPP

#include <dice/template-library/control_flow.hpp>
#include <dice/template-library/overloaded.hpp>

#include <concepts>
#include <optional>

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
    } // detail_try_traits

    /**
     * A type that can be used in try_* algorithms.
     */
    template<typename T>
    concept try_result = requires (T self, typename try_traits<T>::output_type out) {
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
         * Construct self from the output_type.
         */
        { try_traits<T>::from_output(std::move(out)) } -> std::same_as<T>;

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


    template<typename B, typename C>
    struct try_traits<control_flow<B, C>> {
        using self_type = control_flow<B, C>;
        using output_type = C;
        using residual_type = control_flow<B, void>;

        template<typename Out>
        using rebind_output = control_flow<B, Out>;

        static constexpr self_type from_output(output_type &&out) {
            return cfcontinue{std::move(out)};
        }

        static constexpr control_flow<residual_type, output_type> branch(self_type &&self) {
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

        static constexpr self_type from_output(output_type &&out) {
            return self_type{std::move(out)};
        }

        static constexpr control_flow<residual_type, output_type> branch(self_type &&self) {
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

        static constexpr self_type from_output(output_type &&out) {
            return self_type{std::in_place, std::move(out)};
        }

        static constexpr control_flow<residual_type, output_type> branch(self_type &&self) {
            if (self.has_value()) {
                return control_flow<residual_type, output_type>{in_place_continue, std::move(*self)};
            }

            return control_flow<residual_type, output_type>{in_place_break, std::unexpected{std::move(self.error())}};
        }
    };
    static_assert(try_result<std::expected<int, int>>);
#endif // __cpp_lib_expected >= 202202L

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_TRYTRAITS_HPP
