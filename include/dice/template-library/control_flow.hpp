#ifndef DICE_TEMPLATELIBRARY_CONTROLFLOW_HPP
#define DICE_TEMPLATELIBRARY_CONTROLFLOW_HPP

#include <dice/template-library/variant2.hpp>
#include <dice/template-library/type_traits.hpp>

namespace dice::template_library {
    /**
     * The break arm of a control_flow: stop the computation and propagate value to the caller.
     */
    template<typename T>
    struct cfbreak {
        T value;

        bool operator==(cfbreak const &other) const = default;
        auto operator<=>(cfbreak const &other) const = default;
    };

    template<>
    struct cfbreak<void> {
        bool operator==(cfbreak const &other) const = default;
        auto operator<=>(cfbreak const &other) const = default;
    };

    /**
     * The continue arm of a control_flow: carry on with value.
     * Defaults to void for computations that have nothing to carry (see try_for_each).
     */
    template<typename T = void>
    struct cfcontinue {
        T value;

        bool operator==(cfcontinue const &other) const = default;
        auto operator<=>(cfcontinue const &other) const = default;
    };

    template<>
    struct cfcontinue<void> {
        bool operator==(cfcontinue const &other) const = default;
        auto operator<=>(cfcontinue const &other) const = default;
    };


    struct in_place_break_t {};
    inline constexpr in_place_break_t in_place_break{};

    struct in_place_continue_t {};
    inline constexpr in_place_continue_t in_place_continue{};

    /**
     * Rust like ControlFlow: holds either a cfbreak<B>, telling the caller to stop and propagate the break value,
     * or a cfcontinue<C>, telling it to carry on with the continue value. Used by the try_* range algorithms to
     * decide whether to keep iterating (see try_traits::branch).
     *
     * Either arm may be void to express that there is no value. Similar to std::expected<void, E>.
     *
     * @tparam B break type, void if the computation cannot break
     * @tparam C continue type, void if the computation cannot continue
     */
    template<typename B, typename C = void>
    struct control_flow {
    private:
        template<typename, typename>
        friend struct control_flow;

        static constexpr size_t continue_index = 0;
        static constexpr size_t break_index = 1;

        variant2<cfcontinue<C>, cfbreak<B>> data_;

    public:
        constexpr control_flow() noexcept requires (std::is_void_v<C>) = default;

        template<typename U = std::remove_cv_t<C>> requires (!std::is_void_v<C>)
        explicit(!std::is_convertible_v<U, C>) constexpr control_flow(U &&cont)
            : control_flow{in_place_continue, std::forward<U>(cont)} {
        }

        constexpr control_flow(cfcontinue<C> const &cont)
            : data_{cont} {
        }
        constexpr control_flow(cfcontinue<C> &&cont)
            : data_{std::move(cont)} {
        }

        constexpr control_flow(cfbreak<B> const &brk)
            : data_{brk} {
        }
        constexpr control_flow(cfbreak<B> &&brk)
            : data_{std::move(brk)} {
        }

        template<typename ...Args>
        explicit constexpr control_flow(in_place_continue_t, Args &&...args)
            : data_{std::in_place_index<continue_index>, std::forward<Args>(args)...}
        {
        }

        template<typename ...Args>
        explicit constexpr control_flow(in_place_break_t, Args &&...args)
            : data_{std::in_place_index<break_index>, std::forward<Args>(args)...}
        {
        }

        template<typename B2, typename C2> requires (std::is_convertible_v<B2, B> && std::is_convertible_v<C2, C>)
        constexpr control_flow(control_flow<B2, C2> const &cf)
            : data_{cf}
        {
        }

        template<typename B2, typename C2> requires (std::is_convertible_v<B2, B> && std::is_convertible_v<C2, C>)
        constexpr control_flow(control_flow<B2, C2> &&cf)
            : data_{std::move(cf)}
        {
        }

        [[nodiscard]] constexpr bool is_continue() const noexcept {
            return data_.index() == continue_index;
        }

        [[nodiscard]] constexpr bool is_break() const noexcept {
            return data_.index() == break_index;
        }

        template<typename Self>
        [[nodiscard]] constexpr decltype(auto) get_continue(this Self &&self) {
            if constexpr (std::is_void_v<C>) {
                return;
            } else {
                return dice::template_library::forward_like<Self>(get<cfcontinue<C>>(self.data_).value);
            }
        }

        template<typename Self>
        [[nodiscard]] constexpr decltype(auto) get_break(this Self &&self) {
            if constexpr (std::is_void_v<B>) {
                return;
            } else {
                return dice::template_library::forward_like<Self>(get<cfbreak<B>>(self.data_).value);
            }
        }

        template<typename Self, typename F> requires (std::is_same_v<std::remove_cvref_t<Self>, control_flow>)
        friend constexpr decltype(auto) visit(F &&visitor, Self &&self) {
            return visit(std::forward<F>(visitor), dice::template_library::forward_like<Self>(self.data_));
        }

        bool operator==(control_flow const &other) const = default;
        auto operator<=>(control_flow const &other) const = default;
    };

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_CONTROLFLOW_HPP
