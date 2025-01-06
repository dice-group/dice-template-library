#ifndef DICE_TEMPLATELIBRARY_VARIANT2_HPP
#define DICE_TEMPLATELIBRARY_VARIANT2_HPP

#include <cassert>
#include <compare>
#include <cstdint>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

#define DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(noexcept_spec, action) \
	if constexpr (noexcept_spec) {                                      \
		(action);                                                       \
	} else {                                                            \
		try {                                                           \
			(action);                                                   \
		} catch (...) {                                                 \
			discriminant_ = discriminant_type::ValuelessByException;    \
			throw;                                                      \
		}                                                               \
	}


namespace dice::template_library {
    template<typename T, typename U>
    struct variant2;
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
            return visit([]<typename X>(X const &x) noexcept {
                // this is as far as I can tell what std::variant does
                if constexpr (std::is_same_v<X, T>) {
                    return hash<size_t>{}(0) + hash<T>{}(x);
                } else {
                    return hash<size_t>{}(1) + hash<U>{}(x);
                }
            }, var);
        }
    };
} // namespace std

namespace dice::template_library {
    namespace detail_variant2 {
        template<typename From, typename To>
        struct copy_const {
            using type = To;
        };

        template<typename From, typename To>
        struct copy_const<From const, To> {
            using type = std::add_const_t<To>;
        };

        template<typename From, typename To>
        struct copy_reference {
            using type = To;
        };

        template<typename From, typename To>
        struct copy_reference<From &, To> {
            using type = std::add_lvalue_reference_t<To>;
        };

        template<typename From, typename To>
        struct copy_reference<From &&, To> {
            using type = std::add_rvalue_reference_t<To>;
        };

        template<typename From, typename To>
        using copy_cvref_t = typename copy_reference<From, typename copy_const<std::remove_reference_t<From>, To>::type>::type;

        template<typename ...Ts>
        struct select_variant {
            using type = std::variant<Ts...>;
        };

        template<typename T, typename U>
        struct select_variant<T, U> {
            using type = ::dice::template_library::variant2<T, U>;
        };

        template<typename Self, typename F>
        constexpr decltype(auto) visit_impl(Self &&self, F &&visitor) {
            using discriminant_type = typename std::remove_cvref_t<Self>::discriminant_type;

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

        template<size_t ix, typename Self> requires (ix <= 1)
        constexpr auto get_impl(Self &&self) -> copy_cvref_t<decltype(std::forward<Self>(self)), std::variant_alternative_t<ix, std::remove_cvref_t<Self>>> {
            using discriminant_type = typename std::remove_cvref_t<Self>::discriminant_type;

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

        template<typename X, typename Self> requires (std::is_same_v<X, typename std::remove_cvref_t<Self>::first_type> || std::is_same_v<X, typename std::remove_cvref_t<Self>::second_type>)
        constexpr decltype(auto) get_impl(Self &&self) {
            return get_impl<std::is_same_v<X, typename std::remove_cvref_t<Self>::first_type> ? 0 : 1>(std::forward<Self>(self));
        }

        template<size_t ix, typename Self> requires (ix <= 1)
        constexpr auto get_if_impl(Self &&self) noexcept -> typename copy_const<std::remove_reference_t<Self>, std::variant_alternative_t<ix, std::remove_cvref_t<Self>>>::type * {
            using discriminant_type = typename std::remove_cvref_t<Self>::discriminant_type;

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

        template<typename X, typename Self> requires (std::is_same_v<X, typename std::remove_cvref_t<Self>::first_type> || std::is_same_v<X, typename std::remove_cvref_t<Self>::second_type>)
        constexpr auto *get_if_impl(Self &&self) noexcept {
            return get_if_impl<std::is_same_v<X, typename std::remove_cvref_t<Self>::first_type> ? 0 : 1>(std::forward<Self>(self));
        }
    } // namespace detail_variant2

    using std::variant_npos;
    using std::variant_alternative;
    using std::variant_alternative_t;
    using std::variant_size;
    using std::variant_size_v;
    using std::bad_variant_access;

	/**
     * An optimized version of std::variant for 2 types
     * @tparam T first type
     * @tparam U second type
     */
    template<typename T, typename U>
    struct variant2 {
        static_assert(!std::is_same_v<T, U>, "Multiple occurrences of the same type are not supported");

        using first_type = T;
        using second_type = U;

    private:
        enum struct discriminant_type : uint8_t {
            First = 0,
            Second = 1,
            ValuelessByException = 2,
        };

        union {
            T a_; ///< active if discriminant_ == discriminant_type::First
            U b_; ///< active if discriminant_ == discriminant_type::Second
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
                            DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(std::is_nothrow_copy_constructible_v<U>, {
                                new (&b_) U{other.b_};
                            });
                            break;
                        }
                        case discriminant_type::ValuelessByException: {
                            a_.~T();
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
                            DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(std::is_nothrow_copy_constructible_v<T>, {
                                new (&a_) T{other.a_};
                            });
                            break;
                        }
                        case discriminant_type::Second: {
                            b_ = other.b_;
                            break;
                        }
                        case discriminant_type::ValuelessByException: {
                            b_.~U();
                            break;
                        }
                        default: {
                            assert(false);
                            __builtin_unreachable();
                        }
                    }
                    break;
                }
                case discriminant_type::ValuelessByException: {
                    switch (other.discriminant_) {
                        case discriminant_type::First: {
                            new (&a_) T{other.a_};
                            break;
                        }
                        case discriminant_type::Second: {
                            new (&b_) U{other.b_};
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
                            DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(std::is_nothrow_move_constructible_v<U>, {
                                new (&b_) U{std::move(other.b_)};
                            });
                            break;
                        }
                        case discriminant_type::ValuelessByException: {
                            a_.~T();
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
                            DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(std::is_nothrow_move_constructible_v<T>, {
                                new (&a_) T{std::move(other.a_)};
                            });
                            break;
                        }
                        case discriminant_type::Second: {
                            b_ = std::move(other.b_);
                            break;
                        }
                        case discriminant_type::ValuelessByException: {
                            b_.~U();
                            break;
                        }
                        default: {
                            assert(false);
                            __builtin_unreachable();
                        }
                    }
                    break;
                }
                case discriminant_type::ValuelessByException: {
                    switch (other.discriminant_) {
                        case discriminant_type::First: {
                            new (&a_) T{std::move(other.a_)};
                            break;
                        }
                        case discriminant_type::Second: {
                            new (&b_) U{std::move(other.b_)};
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
                    DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(std::is_nothrow_copy_constructible_v<T>, {
                        new (&a_) T{value};
                    });
                    discriminant_ = discriminant_type::First;
                    break;
                }
                case discriminant_type::ValuelessByException: {
                    new (&a_) T{value};
                    discriminant_ = discriminant_type::First;
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
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
                    DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(std::is_nothrow_move_constructible_v<T>, {
                        new (&a_) T{std::move(value)};
                    });
                    discriminant_ = discriminant_type::First;
                    break;
                }
                case discriminant_type::ValuelessByException: {
                    new (&a_) T{std::move(value)};
                    discriminant_ = discriminant_type::First;
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
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
                    DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(std::is_nothrow_copy_constructible_v<U>, {
                        new (&b_) U{value};
                    });
                    discriminant_ = discriminant_type::Second;
                    break;
                }
                case discriminant_type::Second: {
                    b_ = value;
                    break;
                }
                case discriminant_type::ValuelessByException: {
                    new (&b_) U{value};
                    discriminant_ = discriminant_type::Second;
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
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
                    DICE_TEMPLATELIBRARY_DETAIL_VARIANT2_TRY(std::is_nothrow_move_constructible_v<U>, {
                        new (&b_) U{std::move(value)};
                    });
                    discriminant_ = discriminant_type::Second;
                    break;
                }
                case discriminant_type::Second: {
                    b_ = std::move(value);
                    break;
                }
                case discriminant_type::ValuelessByException: {
                    new (&b_) U{std::move(value)};
                    discriminant_ = discriminant_type::Second;
                    break;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
            return *this;
        }

        [[nodiscard]] constexpr bool valueless_by_exception() const noexcept {
            return discriminant_ == discriminant_type::ValuelessByException;
        }

        [[nodiscard]] constexpr size_t index() const noexcept {
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

        template<typename X, typename ...Args> requires (std::is_same_v<X, T> || std::is_same_v<X, U>)
        constexpr X &emplace(Args &&...args) {
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
                        case discriminant_type::ValuelessByException: {
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
                        case discriminant_type::ValuelessByException: {
                            new (&b_) U{std::forward<Args>(args)...};
                            discriminant_ = discriminant_type::Second;
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
                throw;
            }
        }

        template<size_t ix, typename ...Args> requires (ix <= 1)
        constexpr std::variant_alternative_t<ix, variant2> &emplace(Args &&...args) {
            if constexpr (ix == 0) {
                return emplace<T>(std::forward<Args>(args)...);
            } else {
                return emplace<U>(std::forward<Args>(args)...);
            }
        }

        friend constexpr void swap(variant2 &lhs, variant2 &rhs) noexcept(std::is_nothrow_move_assignable_v<variant2>) {
            variant2 tmp = std::move(lhs);
            lhs = std::move(rhs);
            rhs = std::move(tmp);
        }

        constexpr bool operator==(variant2 const &other) const noexcept {
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

        constexpr auto operator<=>(variant2 const &other) const noexcept {
            using ret_type = std::common_comparison_category_t<std::compare_three_way_result_t<T>, std::compare_three_way_result_t<U>>;

            if (discriminant_ != other.discriminant_) {
                if (discriminant_ == discriminant_type::ValuelessByException) {
                    return ret_type::less;
                }

                if (other.discriminant_ == discriminant_type::ValuelessByException) {
                    return ret_type::greater;
                }

                return static_cast<ret_type>(discriminant_ <=> other.discriminant_);
            }

            switch (discriminant_) {
                case discriminant_type::First: {
                    return static_cast<ret_type>(a_ <=> other.a_);
                }
                case discriminant_type::Second: {
                    return static_cast<ret_type>(b_ <=> other.b_);
                }
                case discriminant_type::ValuelessByException: {
                    return ret_type::equivalent;
                }
                default: {
                    assert(false);
                    __builtin_unreachable();
                }
            }
        }

        template<typename Self, typename F>
        friend constexpr decltype(auto) detail_variant2::visit_impl(Self &&self, F &&visitor);

        template<size_t ix, typename Self> requires (ix <= 1)
        friend constexpr auto detail_variant2::get_impl(Self &&self) -> detail_variant2::copy_cvref_t<decltype(std::forward<Self>(self)), std::variant_alternative_t<ix, std::remove_cvref_t<Self>>>;

        template<size_t ix, typename Self> requires (ix <= 1)
        friend constexpr auto detail_variant2::get_if_impl(Self &&self) noexcept -> typename detail_variant2::copy_const<std::remove_reference_t<Self>, std::variant_alternative_t<ix, std::remove_cvref_t<Self>>>::type *;
    };

    template<typename X, typename T, typename U>
    [[nodiscard]] constexpr bool holds_alternative(variant2<T, U> const &var) noexcept {
        if constexpr (std::is_same_v<X, T> || std::is_same_v<X, U>) {
            return var.index() == (std::is_same_v<X, T> ? 0 : 1);
        }

        return false;
    }

    // overloads for get<index>
    template<size_t ix, typename T, typename U>
    [[nodiscard]] constexpr std::variant_alternative_t<ix, variant2<T, U>> const &get(variant2<T, U> const &var) {
        return detail_variant2::get_impl<ix>(var);
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] constexpr std::variant_alternative_t<ix, variant2<T, U>> &get(variant2<T, U> &var) {
        return detail_variant2::get_impl<ix>(var);
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] constexpr std::variant_alternative_t<ix, variant2<T, U>> const &&get(variant2<T, U> const &&var) {
        return detail_variant2::get_impl<ix>(std::move(var));
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] constexpr std::variant_alternative_t<ix, variant2<T, U>> &&get(variant2<T, U> &&var) {
        return detail_variant2::get_impl<ix>(std::move(var));
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] constexpr std::variant_alternative_t<ix, variant2<T, U>> const *get_if(variant2<T, U> const *var) noexcept {
        if (var == nullptr) {
            return nullptr;
        }

        return detail_variant2::get_if_impl<ix>(*var);
    }

    template<size_t ix, typename T, typename U>
    [[nodiscard]] constexpr std::variant_alternative_t<ix, variant2<T, U>> *get_if(variant2<T, U> *var) noexcept {
        if (var == nullptr) {
            return nullptr;
        }

        return detail_variant2::get_if_impl<ix>(*var);
    }

    // overloads of get<type>
    template<typename X, typename T, typename U>
    [[nodiscard]] constexpr X const &get(variant2<T, U> const &var) {
        return detail_variant2::get_impl<X>(var);
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] constexpr X &get(variant2<T, U> &var) {
        return detail_variant2::get_impl<X>(var);
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] constexpr X const &&get(variant2<T, U> const &&var) {
        return detail_variant2::get_impl<X>(std::move(var));
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] constexpr X &&get(variant2<T, U> &&var) {
        return detail_variant2::get_impl<X>(std::move(var));
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] constexpr X const *get_if(variant2<T, U> const *var) noexcept {
        if (var == nullptr) {
            return nullptr;
        }

        return detail_variant2::get_if_impl<X>(*var);
    }

    template<typename X, typename T, typename U>
    [[nodiscard]] constexpr X *get_if(variant2<T, U> *var) noexcept {
        if (var == nullptr) {
            return nullptr;
        }

        return detail_variant2::get_if_impl<X>(*var);
    }


    // overloads for visit
    template<typename F, typename T, typename U>
    constexpr decltype(auto) visit(F &&visitor, variant2<T, U> const &var) {
        return detail_variant2::visit_impl(var, std::forward<F>(visitor));
    }

    template<typename F, typename T, typename U>
    constexpr decltype(auto) visit(F &&visitor, variant2<T, U> &var) {
        return detail_variant2::visit_impl(var, std::forward<F>(visitor));
    }

    template<typename F, typename T, typename U>
    constexpr decltype(auto) visit(F &&visitor, variant2<T, U> const &&var) {
        return detail_variant2::visit_impl(std::move(var), std::forward<F>(visitor));
    }

    template<typename F, typename T, typename U>
    constexpr decltype(auto) visit(F &&visitor, variant2<T, U> &&var) {
        return detail_variant2::visit_impl(std::move(var), std::forward<F>(visitor));
    }

    /**
     * Automatically select the fastest variant for the given types
     * if sizeof...(Ts) == 2 selects dice::template_library::variant2<Ts...>
     * otherwise selects std::variant<Ts...>
     */
    template<typename ...Ts>
    using variant = typename detail_variant2::select_variant<Ts...>::type;

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_VARIANT2_HPP
