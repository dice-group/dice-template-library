#ifndef DICE_TEMPLATELIBRARY_VARIANT2_HPP
#define DICE_TEMPLATELIBRARY_VARIANT2_HPP

#include <cassert>
#include <cstdint>
#include <exception>
#include <functional>
#include <type_traits>
#include <variant>

namespace dice::template_library {

	/**
     * An optimized version of std::variant for 2 types
     * @tparam T first type
     * @tparam U second type
     */
    template<typename T, typename U>
    struct variant2 {
        static_assert(!std::is_same_v<T, U>, "Multiple occurrences of the same type are not supported");

    private:
        enum struct discriminant_type : uint8_t {
            First = 0,
            Second = 1,
            ValuelessByException = 2,
        };

        union {
            T a_; ///< active if discriminant == false
            U b_; ///< active if discriminamt == true
        };
        discriminant_type discriminant_;

    public:
        constexpr variant2() noexcept(std::is_nothrow_default_constructible_v<T>)
            : discriminant_{discriminant_type::First} {
            new (&a_) T{};
        }

        constexpr variant2(variant2 const &other) noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<U>)
            : discriminant_{other.discriminant_} {
            switch (discriminant_) {
                case discriminant_type::First: {
                    new (&a_) T{other.a_};
                    break;
                }
                case discriminant_type::Second: {
                    new (&b_) U{other.b_};
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
        }

        constexpr variant2(variant2 &&other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<U>)
            : discriminant_{other.discriminant_} {
            switch (discriminant_) {
                case discriminant_type::First: {
                    new (&a_) T{std::move(other.a_)};
                    break;
                }
                case discriminant_type::Second: {
                    new (&b_) U{std::move(other.b_)};
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
        }

        constexpr variant2(T const &value) noexcept(std::is_nothrow_copy_constructible_v<T>)
            : discriminant_{discriminant_type::First} {
            new (&a_) T{value};
        }

        constexpr variant2(T &&value) noexcept(std::is_nothrow_copy_constructible_v<T>)
            : discriminant_{discriminant_type::First} {
            new (&a_) T{std::move(value)};
        }

        constexpr variant2(U const &value) noexcept(std::is_nothrow_copy_constructible_v<U>)
            : discriminant_{discriminant_type::Second} {
            new (&b_) U{value};
        }

        constexpr variant2(U &&value) noexcept(std::is_nothrow_copy_constructible_v<U>)
            : discriminant_{discriminant_type::Second} {
            new (&b_) U{std::move(value)};
        }

        template<typename ...Args>
        constexpr explicit variant2(std::in_place_type_t<T>, Args &&...args) noexcept(std::is_nothrow_constructible_v<T, decltype(std::forward<Args>(args))...>)
            : discriminant_{discriminant_type::First} {
            new (&a_) T{std::forward<Args>(args)...};
        }

        template<typename ...Args>
        constexpr explicit variant2(std::in_place_type_t<U>, Args &&...args) noexcept(std::is_nothrow_constructible_v<U, decltype(std::forward<Args>(args))...>)
            : discriminant_{discriminant_type::Second} {
            new (&b_) U{std::forward<Args>(args)...};
        }

        template<typename ...Args>
        constexpr explicit variant2(std::in_place_index_t<0>, Args &&...args) noexcept(std::is_nothrow_constructible_v<T, decltype(std::forward<Args>(args))...>)
            : discriminant_{discriminant_type::First} {
            new (&a_) T{std::forward<Args>(args)...};
        }

        template<typename ...Args>
        constexpr explicit variant2(std::in_place_index_t<1>, Args &&...args) noexcept(std::is_nothrow_constructible_v<U, decltype(std::forward<Args>(args))...>)
            : discriminant_{discriminant_type::Second} {
            new (&b_) U{std::forward<Args>(args)...};
        }

        constexpr ~variant2() noexcept(std::is_nothrow_destructible_v<T> && std::is_nothrow_destructible_v<U>) {
            switch (discriminant_) {
                case discriminant_type::First: {
                    a_.~T();
                    break;
                }
                case discriminant_type::Second: {
                    b_.~U();
                    break;
                }
                case discriminant_type::ValuelessByException: {
                    // noop
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
        }

        constexpr variant2 &operator=(variant2 const &other) noexcept(std::is_nothrow_copy_assignable_v<T>
                                                                        && std::is_nothrow_destructible_v<T>
                                                                        && std::is_nothrow_copy_constructible_v<T>
                                                                        && std::is_nothrow_copy_assignable_v<U>
                                                                        && std::is_nothrow_destructible_v<U>
                                                                        && std::is_nothrow_copy_constructible_v<U>) {
            if (this == &other) [[unlikely]] {
                return *this;
            }

            switch (discriminant_) {
                case discriminant_type::First: {
                    switch (other.discriminant_) {
                        case discriminant_type::First: {
                            a_ = other.a_;
                            break;
                        }
                        case discriminant_type::Second: {
                            a_.~T();
                            try {
                                new (&b_) U{other.b_};
                            } catch (...) {
                                discriminant_ = discriminant_type::ValuelessByException;
                                std::rethrow_exception(std::current_exception());
                            }
                            break;
                        }
                        default: {
                            assert(false);
                            __builtin_unreachable();
                        }
                    }
                    break;
                }
                case discriminant_type::Second: {
                    switch (other.discriminant_) {
                        case discriminant_type::First: {
                            b_.~U();
                            try {
                                new (&a_) T{other.a_};
                            } catch (...) {
                                discriminant_ = discriminant_type::ValuelessByException;
                                std::rethrow_exception(std::current_exception());
                            }
                            break;
                        }
                        case discriminant_type::Second: {
                            b_ = other.b_;
                            break;
                        }
                        default: {
                            assert(false);
                            __builtin_unreachable();
                        }
                    }
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }

            discriminant_ = other.discriminant_;

            return *this;
        }

        constexpr variant2 &operator=(variant2 &&other) noexcept(std::is_nothrow_move_assignable_v<T>
                                                                    && std::is_nothrow_destructible_v<T>
                                                                    && std::is_nothrow_move_constructible_v<T>
                                                                    && std::is_nothrow_move_assignable_v<U>
                                                                    && std::is_nothrow_destructible_v<U>
                                                                    && std::is_nothrow_move_constructible_v<U>) {
            assert(this != &other);

            switch (discriminant_) {
                case discriminant_type::First: {
                    switch (other.discriminant_) {
                        case discriminant_type::First: {
                            a_ = std::move(other.a_);
                            break;
                        }
                        case discriminant_type::Second: {
                            a_.~T();
                            try {
                                new (&b_) U{std::move(other.b_)};
                            } catch (...) {
                                discriminant_ = discriminant_type::ValuelessByException;
                                std::rethrow_exception(std::current_exception());
                            }
                            break;
                        }
                        default: {
                            assert(false);
                            __builtin_unreachable();
                        }
                    }
                    break;
                }
                case discriminant_type::Second: {
                    switch (other.discriminant_) {
                        case discriminant_type::First: {
                            b_.~U();
                            try {
                                new (&a_) T{std::move(other.a_)};
                            } catch (...) {
                                discriminant_ = discriminant_type::ValuelessByException;
                                std::rethrow_exception(std::current_exception());
                            }
                            break;
                        }
                        case discriminant_type::Second: {
                            b_ = std::move(other.b_);
                            break;
                        }
                        default: {
                            assert(false);
                            __builtin_unreachable();
                        }
                    }
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }

            discriminant_ = other.discriminant_;

            return *this;
        }

        constexpr variant2 &operator=(T const &value) noexcept(std::is_nothrow_copy_assignable_v<T>
                                                                && std::is_nothrow_destructible_v<U>
                                                                && std::is_nothrow_copy_constructible_v<T>) {

            switch (discriminant_) {
                case discriminant_type::First: {
                    a_ = value;
                    break;
                }
                case discriminant_type::Second: {
                    b_.~U();
                    try {
                        new (&a_) T{value};
                    } catch (...) {
                        discriminant_ = discriminant_type::ValuelessByException;
                        std::rethrow_exception(std::current_exception());
                    }
                    discriminant_ = discriminant_type::First;
                    break;
                }
            }

            return *this;
        }

        constexpr variant2 &operator=(T &&value) noexcept(std::is_nothrow_move_assignable_v<T>
                                                            && std::is_nothrow_destructible_v<U>
                                                            && std::is_nothrow_move_constructible_v<T>) {

            switch (discriminant_) {
                case discriminant_type::First: {
                    a_ = std::move(value);
                    break;
                }
                case discriminant_type::Second: {
                    b_.~U();
                    try {
                        new (&a_) T{std::move(value)};
                    } catch (...) {
                        discriminant_ = discriminant_type::ValuelessByException;
                        std::rethrow_exception(std::current_exception());
                    }
                    discriminant_ = discriminant_type::First;
                    break;
                }
            }

            return *this;
        }

        constexpr variant2 &operator=(U const &value) noexcept(std::is_nothrow_copy_assignable_v<U>
                                                                && std::is_nothrow_destructible_v<T>
                                                                && std::is_nothrow_copy_constructible_v<U>) {
            switch (discriminant_) {
                case discriminant_type::First: {
                    a_.~T();
                    try {
                        new (&b_) U{value};
                    } catch (...) {
                        discriminant_ = discriminant_type::ValuelessByException;
                        std::rethrow_exception(std::current_exception());
                    }
                    discriminant_ = discriminant_type::Second;
                    break;
                }
                case discriminant_type::Second: {
                    b_ = value;
                    break;
                }
            }
            return *this;
        }

        constexpr variant2 &operator=(U &&value) noexcept(std::is_nothrow_move_assignable_v<U>
                                                            && std::is_nothrow_destructible_v<T>
                                                            && std::is_nothrow_move_constructible_v<U>) {
            switch (discriminant_) {
                case discriminant_type::First: {
                    a_.~T();
                    try {
                        new (&b_) U{std::move(value)};
                    } catch (...) {
                        discriminant_ = discriminant_type::ValuelessByException;
                        std::rethrow_exception(std::current_exception());
                    }
                    discriminant_ = discriminant_type::Second;
                    break;
                }
                case discriminant_type::Second: {
                    b_ = std::move(value);
                    break;
                }
            }
            return *this;
        }

        [[nodiscard]] bool valueless_by_exception() const noexcept {
            return discriminant_ == discriminant_type::ValuelessByException;
        }

        [[nodiscard]] size_t index() const noexcept {
            switch (discriminant_) {
                case discriminant_type::First: return 0;
                case discriminant_type::Second: return 1;
                case discriminant_type::ValuelessByException: return std::variant_npos;
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
        }

        template<typename X, typename ...Args> requires (std::is_same_v<X, T> || std::is_same_v<U, T>)
        X &emplace(Args &&...args) {
            try {
                if constexpr (std::is_same_v<X, T>) {
                    switch (discriminant_) {
                        case discriminant_type::First: {
                            a_.~T();
                            new (&a_) T{std::forward<Args>(args)...};
                            break;
                        }
                        case discriminant_type::Second: {
                            b_.~U();
                            new (&a_) T{std::forward<Args>(args)...};
                            discriminant_ = discriminant_type::First;
                            break;
                        }
                        default: {
                            assert(false);
                            __builtin_unreachable();
                        }
                    }

                    return a_;
                } else {
                    switch (discriminant_) {
                        case discriminant_type::First: {
                            a_.~T();
                            new (&b_) U{std::forward<Args>(args)...};
                            discriminant_ = discriminant_type::Second;
                            break;
                        }
                        case discriminant_type::Second: {
                            b_.~U();
                            new (&b_) U{std::forward<Args>(args)...};
                            break;
                        }
                        default: {
                            assert(false);
                            __builtin_unreachable();
                        }
                    }

                    return b_;
                }
            } catch (...) {
                discriminant_ = discriminant_type::ValuelessByException;
                std::rethrow_exception(std::current_exception());
            }
        }

        template<size_t ix, typename ...Args> requires (ix <= 1)
        std::conditional_t<ix == 0, T, U> &emplace(Args &&...args) {
            if constexpr (ix == 0) {
                return emplace<T>(std::forward<Args>(args)...);
            } else {
                return emplace<U>(std::forward<Args>(args)...);
            }
        }

        friend void swap(variant2 &lhs, variant2 &rhs) noexcept(std::is_nothrow_move_assignable_v<variant2>) {
            variant2 tmp = std::move(lhs);
            lhs = std::move(rhs);
            rhs = std::move(tmp);
        }

        template<typename X>
        constexpr bool holds_alternative() const noexcept {
            if constexpr (std::is_same_v<X, T> || std::is_same_v<X, U>) {
                return index() == (std::is_same_v<X, T> ? 0 : 1);
            }

            return false;
        }

        template<typename Self, typename F>
        decltype(auto) visit(this Self &&self, F &&visitor) {
            switch (self.discriminant_) {
                case discriminant_type::First: {
                    return std::invoke(std::forward<F>(visitor), std::forward<Self>(self).a_);
                }
                case discriminant_type::Second: {
                    return std::invoke(std::forward<F>(visitor), std::forward<Self>(self).b_);
                }
                case discriminant_type::ValuelessByException: {
                    throw std::bad_variant_access{};
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
        }

        template<typename Self, size_t ix> requires (ix <= 1)
        decltype(auto) get(this Self &&self) {
            if constexpr (ix == 0) {
                if (self.discriminant_ != discriminant_type::First) [[unlikely]] {
                    throw std::bad_variant_access{};
                }

                return std::forward<Self>(self).a_;
            } else { // ix == 1
                if (self.discriminant_ != discriminant_type::Second) [[unlikely]] {
                    throw std::bad_variant_access{};
                }

                return std::forward<Self>(self).b_;
            }
        }

        template<typename Self, typename X> requires (std::is_same_v<X, T> || std::is_same_v<X, U>)
        decltype(auto) get(this Self &&self) {
            return std::forward<Self>(self).template get<std::is_same_v<X, T> ? 0 : 1>();
        }

        template<typename Self, size_t ix> requires (ix <= 1)
        auto *get_if(this Self &&self) noexcept {
            if constexpr (ix == 0) {
                if (self.discriminant_ != discriminant_type::First) [[unlikely]] {
                    return nullptr;
                }

                return &std::forward<Self>(self).a_;
            } else { // ix == 1
                if (self.discriminant_ != discriminant_type::Second) [[unlikely]] {
                    return nullptr;
                }

                return &std::forward<Self>(self).b_;
            }
        }

        template<typename Self, typename X> requires (std::is_same_v<X, T> || std::is_same_v<X, U>)
        auto *get_if(this Self &&self) noexcept {
            return std::forward<Self>(self).template get_if<std::is_same_v<X, T> ? 0 : 1>();
        }

        bool operator==(variant2 const &other) const noexcept {
            if (discriminant_ != other.discriminant_) {
                return false;
            }

            switch (discriminant_) {
                case discriminant_type::First: {
                    return a_ == other.a_;
                }
                case discriminant_type::Second: {
                    return b_ == other.b_;
                }
                case discriminant_type::ValuelessByException: {
                    return true;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
        }

        auto operator<=>(variant2 const &other) const noexcept {
            if (discriminant_ != other.discriminant_) {
                if (discriminant_ == discriminant_type::ValuelessByException) {
                    return std::strong_ordering::less;
                }

                if (other.discriminant_ == discriminant_type::ValuelessByException) {
                    return std::strong_ordering::greater;
                }

                return discriminant_ <=> other.discriminant_;
            }

            switch (discriminant_) {
                case discriminant_type::First: {
                    return a_ <=> other.a_;
                }
                case discriminant_type::Second: {
                    return b_ <=> other.b_;
                }
                case discriminant_type::ValuelessByException: {
                    return std::strong_ordering::equal;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
        }
    };

    template<typename X, typename T, typename U>
    [[nodiscard]] constexpr bool holds_alternative(variant2<T, U> const &var) noexcept {
        return var.template holds_alternative<X>();
    }

    // overloads for get<index>
    template<size_t ix, typename T, typename U>
    [[nodiscard]] decltype(auto) get(variant2<T, U> const &var) {
        return var.template get<ix>();
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] decltype(auto) get(variant2<T, U> &var) {
        return var.template get<ix>();
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] decltype(auto) get(variant2<T, U> const &&var) {
        return std::move(var).template get<ix>();
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] decltype(auto) get(variant2<T, U> &&var) {
        return std::move(var).template get<ix>();
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] auto const *get_if(variant2<T, U> const *var) noexcept {
        if (var == nullptr) {
            return nullptr;
        }

        return var->template get<ix>();
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] auto *get_if(variant2<T, U> *var) noexcept {
        if (var == nullptr) {
            return nullptr;
        }

        return var->template get<ix>();
    }

    // overloads of get<type>
    template<typename X, typename T, typename U>
    [[nodiscard]] decltype(auto) get(variant2<T, U> const &var) {
        return var.template get<X>();
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] decltype(auto) get(variant2<T, U> &var) {
        return var.template get<X>();
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] decltype(auto) get(variant2<T, U> const &&var) {
        return std::move(var).template get<X>();
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] decltype(auto) get(variant2<T, U> &&var) {
        return std::move(var).template get<X>();
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] auto const *get_if(variant2<T, U> const *var) noexcept {
        if (var == nullptr) {
            return nullptr;
        }

        return var->template get<X>();
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] auto *get_if(variant2<T, U> *var) noexcept {
        if (var == nullptr) {
            return nullptr;
        }

        return var->template get<X>();
    }


    // overloads for visit
    template<typename F, typename T, typename U>
    [[nodiscard]] decltype(auto) visit(F &&visitor, variant2<T, U> const &var) {
        return var.visit(std::forward<F>(visitor));
    }

    template<typename F, typename T, typename U>
    [[nodiscard]] decltype(auto) visit(F &&visitor, variant2<T, U> &var) {
        return var.visit(std::forward<F>(visitor));
    }

    template<typename F, typename T, typename U>
    [[nodiscard]] decltype(auto) visit(F &&visitor, variant2<T, U> const &&var) {
        return std::move(var).visit(std::forward<F>(visitor));
    }

    template<typename F, typename T, typename U>
    [[nodiscard]] decltype(auto) visit(F &&visitor, variant2<T, U> &&var) {
        return std::move(var).visit(std::forward<F>(visitor));
    }

} // namespace dice::template_library

namespace std {
    template<typename T, typename U>
    struct variant_size<::dice::template_library::variant2<T, U>> : std::integral_constant<size_t, 2> {
    };

    template<size_t ix, typename T, typename U> requires (ix <= 1)
    struct variant_alternative<ix, ::dice::template_library::variant2<T, U>> : std::conditional<ix == 0, T, U> {
    };

    template<typename T, typename U>
    struct hash<::dice::template_library::variant2<T, U>> {
        [[nodiscard]] size_t operator()(::dice::template_library::variant2<T, U> const &var) const noexcept {
            return var.visit([]<typename X>(X const &x) {
                // this is as far as I can tell what std::variant does
                if constexpr (std::is_same_v<X, T>) {
                    return std::hash<size_t>{}(0) + std::hash<T>{}(x);
                } else {
                    return std::hash<size_t>{}(1) + std::hash<U>{}(x);
                }
            });
        }
    };
} // namespace std

#endif // DICE_TEMPLATELIBRARY_VARIANT2_HPP
