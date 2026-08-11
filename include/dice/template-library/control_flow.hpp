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
    struct cfbreak<void>;

    /**
     * The continue arm of a control_flow: carry on with value.
     * Defaults to std::monostate for computations that have nothing to carry (see try_for_each).
     */
    template<typename T = std::monostate>
    struct cfcontinue {
        T value;

        bool operator==(cfcontinue const &other) const = default;
        auto operator<=>(cfcontinue const &other) const = default;
    };

    template<>
    struct cfcontinue<void>;


    struct in_place_break_t {};
    inline constexpr in_place_break_t in_place_break{};

    struct in_place_continue_t {};
    inline constexpr in_place_continue_t in_place_continue{};


    namespace detail_control_flow {
        template<typename B, typename C>
        struct select_impl {
            static constexpr size_t continue_index = 0;
            static constexpr size_t break_index = 1;
            using type = dice::template_library::variant<cfcontinue<C>, cfbreak<B>>;
        };

        template<typename B>
        struct select_impl<B, void> {
            static constexpr size_t break_index = 0;
            using type = dice::template_library::variant<cfbreak<B>>;
        };

        template<typename C>
        struct select_impl<void, C> {
            static constexpr size_t continue_index = 0;
            using type = dice::template_library::variant<cfcontinue<C>>;
        };
    } // namespace detail_control_flow

    /**
     * Rust like ControlFlow: holds either a cfbreak<B>, telling the caller to stop and propagate the break value,
     * or a cfcontinue<C>, telling it to carry on with the continue value. Used by the try_* range algorithms to
     * decide whether to keep iterating (see try_traits::branch).
     *
     * Either arm may be void to express that it cannot occur, e.g. control_flow<E, void> can only ever break.
     *
     * @tparam B break type, void if the computation cannot break
     * @tparam C continue type, void if the computation cannot continue
     */
    template<typename B, typename C = std::monostate>
    struct control_flow {
        static constexpr bool has_break = !std::is_void_v<B>;
        static constexpr bool has_continue = !std::is_void_v<C>;

    private:
        template<typename, typename>
        friend struct control_flow;

        using select = detail_control_flow::select_impl<B, C>;
        select::type data_;

    public:
        control_flow() = delete;

        constexpr control_flow(cfcontinue<C> const &cont) requires (has_continue)
            : data_{cont} {
        }
        constexpr control_flow(cfcontinue<C> &&cont) requires (has_continue)
            : data_{std::move(cont)} {
        }

        constexpr control_flow(cfbreak<B> const &brk) requires (has_break)
            : data_{brk} {
        }
        constexpr control_flow(cfbreak<B> &&brk) requires (has_break)
            : data_{std::move(brk)} {
        }

        template<typename ...Args> requires (has_continue)
        explicit constexpr control_flow(in_place_continue_t, Args &&...args)
            : data_{std::in_place_index<select::continue_index>, std::forward<Args>(args)...}
        {
        }

        template<typename ...Args> requires (has_break)
        explicit constexpr control_flow(in_place_break_t, Args &&...args)
            : data_{std::in_place_index<select::break_index>, std::forward<Args>(args)...}
        {
        }

        constexpr control_flow(control_flow<B, void> const &other) requires (has_break)
            : control_flow{in_place_break, other.get_break()}
        {
        }

        constexpr control_flow(control_flow<B, void> &&other) requires (has_break)
            : control_flow{in_place_break, std::move(other).get_break()}
        {
        }

        constexpr control_flow(control_flow<void, C> const &other) requires (has_continue)
            : control_flow{in_place_continue, other.get_continue()}
        {
        }

        constexpr control_flow(control_flow<void, C> &&other) requires (has_continue)
            : control_flow{in_place_continue, std::move(other).get_continue()}
        {
        }

        [[nodiscard]] constexpr bool is_continue() const noexcept requires (has_continue) {
            return data_.index() == select::continue_index;
        }

        [[nodiscard]] constexpr bool is_break() const noexcept requires (has_break) {
            return data_.index() == select::break_index;
        }

        template<typename Self>
        [[nodiscard]] constexpr decltype(auto) get_continue(this Self &&self) requires (has_continue) {
            return dice::template_library::forward_like<Self>(get<cfcontinue<C>>(self.data_).value);
        }

        template<typename Self>
        [[nodiscard]] constexpr decltype(auto) get_break(this Self &&self) requires (has_break) {
            return dice::template_library::forward_like<Self>(get<cfbreak<B>>(self.data_).value);
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
