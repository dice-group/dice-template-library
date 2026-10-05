#ifndef DICE_TEMPLATELIBRARY_OPTIONALREF_HPP
#define DICE_TEMPLATELIBRARY_OPTIONALREF_HPP

#include <concepts>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace dice::template_library {

#if __cpp_lib_optional >= 202506L

    /**
     * A reference to a `T` or nothing: `std::optional<T &>`, which this standard library provides.
     */
    template<typename T>
    using optional_ref = std::optional<T &>;

#else  // __cpp_lib_optional >= 202506L

    template<typename T>
    struct optional_ref;

    namespace detail_optional_ref {
        /**
         * True for the result types that `and_then` accepts: `std::optional<U>` and `optional_ref<U>`
         * (which is `std::optional<U &>` with a standard library that provides it).
         */
        template<typename X>
        inline constexpr bool is_optional = false;

        template<typename X>
        inline constexpr bool is_optional<std::optional<X>> = true;

        template<typename X>
        inline constexpr bool is_optional<optional_ref<X>> = true;
    }  // namespace detail_optional_ref

    /**
     * A reference to a `T` or nothing. It stores a pointer to the referenced object.
     *
     * It has the interface of `std::optional<T &>` (C++26), which this standard library does not provide.
     * With a standard library that provides it, `optional_ref<T>` is `std::optional<T &>`.
     *
     * Like `std::optional<T &>`:
     * - It never binds a temporary.
     * - Copy assignment and `emplace` rebind the reference. They never assign to the referenced object.
     * - `optional_ref<T>` converts to `optional_ref<T const>` and to `optional_ref<Base>`.
     * - `==` compares the referenced values, not their addresses.
     * - It is a view and a borrowed range, and `std::format` does not format it as a range.
     *
     * Members of `std::optional<T &>` that this class does not have: construction from `std::optional<U>` and
     * with `std::in_place`, comparison with a value of `T` and with `std::optional<U>`, and the ordering operators.
     *
     * Other differences:
     * - `iterator` is `T *`. For `std::optional<T &>` it is an implementation-defined type, so code like
     *   `T *p = ref.begin();` compiles only with this class.
     * - The language mode chooses the type. With a standard library that provides `std::optional<T &>` only
     *   in C++26 mode, translation units built in C++23 and in C++26 mode see two different types. A function
     *   with an `optional_ref` parameter then does not link across them.
     *
     * @tparam T the type of the referenced object, may be const
     */
    template<typename T>
    struct optional_ref {
        using value_type = T;
        using iterator = T *;

        constexpr optional_ref() noexcept = default;

        constexpr optional_ref(std::nullopt_t /*nullopt*/) noexcept {  // NOLINT(google-explicit-constructor)
        }

        constexpr optional_ref(T &ref) noexcept  // NOLINT(google-explicit-constructor)
            : ptr_(std::addressof(ref)) {
        }

        /**
         * Does not bind a temporary. An rvalue binds to this deleted overload, not to `T &` with a const `T`.
         */
        optional_ref(std::remove_cv_t<T> const &&) = delete;

        /**
         * Converts `optional_ref<U>` into `optional_ref<T>` if a `U *` converts to a `T *`,
         * e.g. `optional_ref<T>` into `optional_ref<T const>`.
         */
        template<typename U>
        requires (!std::is_same_v<U, T> && std::is_convertible_v<U *, T *>)
        constexpr optional_ref(optional_ref<U> const &other) noexcept  // NOLINT(google-explicit-constructor)
            : ptr_(other.has_value() ? std::addressof(*other) : nullptr) {
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return ptr_ != nullptr;
        }

        constexpr explicit operator bool() const noexcept {
            return has_value();
        }

        /**
         * @return the referenced object. The behavior is undefined if there is none.
         */
        [[nodiscard]] constexpr T &operator*() const noexcept {
            return *ptr_;
        }

        /**
         * @return a pointer to the referenced object. The behavior is undefined if there is none.
         */
        [[nodiscard]] constexpr T *operator->() const noexcept {
            return ptr_;
        }

        /**
         * @return the referenced object
         * @throws std::bad_optional_access if there is none
         */
        [[nodiscard]] constexpr T &value() const {
            if (!has_value()) {
                throw std::bad_optional_access{};
            }
            return *ptr_;
        }

        /**
         * @return a copy of the referenced object, or `default_value` converted to `std::remove_cv_t<T>`
         */
        template<typename U = std::remove_cv_t<T>>
        requires std::is_convertible_v<U, std::remove_cv_t<T>>
        [[nodiscard]] constexpr std::remove_cv_t<T> value_or(U &&default_value) const {
            if (has_value()) {
                return *ptr_;
            }
            return static_cast<std::remove_cv_t<T>>(std::forward<U>(default_value));
        }

        /**
         * `f` must return a `std::optional` or an `optional_ref`.
         * @return `f(**this)` if there is a value, otherwise an empty value of the result type of `f`
         */
        template<typename F>
        requires detail_optional_ref::is_optional<std::remove_cvref_t<std::invoke_result_t<F, T &>>>
        constexpr auto and_then(F &&f) const {
            using result_type = std::remove_cvref_t<std::invoke_result_t<F, T &>>;
            if (has_value()) {
                return std::invoke(std::forward<F>(f), *ptr_);
            }
            return result_type{};
        }

        /**
         * @return `f(**this)` in an optional if there is a value, otherwise an empty optional.
         * If `f` returns an lvalue reference, the result is an `optional_ref`, otherwise a `std::optional`.
         */
        template<typename F>
        constexpr auto transform(F &&f) const {
            using result_type = std::invoke_result_t<F, T &>;
            if constexpr (std::is_lvalue_reference_v<result_type>) {
                using optional_type = optional_ref<std::remove_reference_t<result_type>>;
                if (has_value()) {
                    return optional_type{std::invoke(std::forward<F>(f), *ptr_)};
                }
                return optional_type{};
            } else {
                using optional_type = std::optional<std::remove_cv_t<result_type>>;
                if (has_value()) {
                    return optional_type{std::invoke(std::forward<F>(f), *ptr_)};
                }
                return optional_type{};
            }
        }

        /**
         * @return `*this` if there is a value, otherwise `f()`
         */
        template<typename F>
        requires std::same_as<std::remove_cvref_t<std::invoke_result_t<F>>, optional_ref>
        constexpr optional_ref or_else(F &&f) const {
            if (has_value()) {
                return *this;
            }
            return std::invoke(std::forward<F>(f));
        }

        /**
         * Rebinds the reference to `ref`.
         * @return `ref`
         */
        constexpr T &emplace(T &ref) noexcept {
            ptr_ = std::addressof(ref);
            return ref;
        }

        /**
         * Does not bind a temporary. An rvalue binds to this deleted overload, not to `T &` with a const `T`.
         */
        T &emplace(std::remove_cv_t<T> const &&) = delete;

        constexpr void reset() noexcept {
            ptr_ = nullptr;
        }

        constexpr void swap(optional_ref &other) noexcept {
            std::swap(ptr_, other.ptr_);
        }

        /**
         * Iterates over the referenced object, if there is one.
         */
        [[nodiscard]] constexpr iterator begin() const noexcept {
            return ptr_;
        }

        [[nodiscard]] constexpr iterator end() const noexcept {
            return ptr_ == nullptr ? nullptr : ptr_ + 1;
        }

        friend constexpr bool operator==(optional_ref const &lhs, std::nullopt_t /*nullopt*/) noexcept {
            return !lhs.has_value();
        }

        /**
         * Compares the referenced values, like `std::optional` does. Two empty values are equal.
         * It exists only if `*lhs == *rhs` compiles and converts to `bool`.
         */
        template<typename U>
        requires requires (T &lhs_value, U &rhs_value) {
            {
                lhs_value == rhs_value
            } -> std::convertible_to<bool>;
        }
        friend constexpr bool operator==(optional_ref const &lhs, optional_ref<U> const &rhs) {
            if (lhs.has_value() != rhs.has_value()) {
                return false;
            }
            return !lhs.has_value() || *lhs == *rhs;
        }

    private:
        T *ptr_ = nullptr;
    };

#endif  // __cpp_lib_optional >= 202506L

}  // namespace dice::template_library

#if __cpp_lib_optional < 202506L

/**
 * Like `std::optional<T &>`, `optional_ref` is a view.
 */
template<typename T>
inline constexpr bool std::ranges::enable_view<dice::template_library::optional_ref<T>> = true;

/**
 * Like `std::optional<T &>`, `optional_ref` is a borrowed range: its iterators point to the referenced object.
 */
template<typename T>
inline constexpr bool std::ranges::enable_borrowed_range<dice::template_library::optional_ref<T>> = true;

#if __cpp_lib_format_ranges
/**
 * Like `std::optional<T &>`, `optional_ref` is not formatted as a range.
 */
template<typename T>
inline constexpr std::range_format std::format_kind<dice::template_library::optional_ref<T>> = std::range_format::disabled;
#endif  // __cpp_lib_format_ranges

#endif  // __cpp_lib_optional < 202506L

#endif  // DICE_TEMPLATELIBRARY_OPTIONALREF_HPP
