#ifndef DICE_TEMPLATELIBRARY_MUTEX_HPP
#define DICE_TEMPLATELIBRARY_MUTEX_HPP

#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	/**
     * An RAII guard for a value behind a mutex.
     * When this mutex_guard is dropped the lock is automatically released.
     *
     * @note to use this correctly it is important to understand that unlike rust, C++ cannot
     *       enforce that you do not use pointers given out by this type beyond its lifetime.
     *       You may not store pointers or references given out via this wrapper beyond its lifetime, otherwise behaviour is undefined.
     *
     * @tparam T the value type protected by the mutex
     * @tparam Mutex the mutex type
     */
    template<typename T, typename Mutex = std::mutex>
    struct mutex_guard {
        using value_type = T;
        using mutex_type = Mutex;

    private:
        T *value_ptr_;
        std::unique_lock<mutex_type> lock_;

    public:
        mutex_guard(std::unique_lock<mutex_type> &&lock, T &value) noexcept
            : value_ptr_{&value},
              lock_{std::move(lock)} {
        }

        mutex_guard() = delete;
        mutex_guard(mutex_guard const &other) noexcept = delete;
        mutex_guard &operator=(mutex_guard const &other) noexcept = delete;
        mutex_guard(mutex_guard &&other) noexcept = default;
        mutex_guard &operator=(mutex_guard &&other) noexcept = default;
        ~mutex_guard() = default;

        value_type *operator->() const noexcept {
            return value_ptr_;
        }

        value_type &operator*() const noexcept {
            return *value_ptr_;
        }
    };

	/**
     * A rust-like mutex type (https://doc.rust-lang.org/std/sync/struct.Mutex.html) that holds its data instead of living next to it.
     *
     * @note Because this is C++ it cannot be fully safe, like the rust version is, for more details see doc comment on mutex_guard.
     * @note This type is non-movable and non-copyable, if you need to do either of these things use `std::unique_ptr<mutex<T>>` or `std::shared_ptr<mutex<T>>`
     *
     * @tparam T value type stored
     * @tparam Mutex the mutex type
     */
    template<typename T, typename Mutex = std::mutex>
    struct mutex {
        using value_type = T;
        using mutex_type = Mutex;

    private:
        value_type value_;
        mutex_type mutex_;

    public:
        constexpr mutex() noexcept(std::is_nothrow_default_constructible_v<value_type>) = default;
        constexpr ~mutex() noexcept(std::is_nothrow_destructible_v<value_type>) = default;

        mutex(mutex const &other) = delete;
        mutex(mutex &&other) = delete;
        mutex &operator=(mutex const &other) = delete;
        mutex &operator=(mutex &&other) = delete;

        explicit constexpr mutex(value_type const &value) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
            : value_{value} {
        }

        explicit constexpr mutex(value_type &&value) noexcept(std::is_nothrow_move_constructible_v<value_type>)
            : value_{std::move(value)} {
        }

        template<typename ...Args>
        explicit constexpr mutex(std::in_place_t, Args &&...args) noexcept(std::is_nothrow_constructible_v<value_type, decltype(std::forward<Args>(args))...>)
            : value_{std::forward<Args>(args)...} {
        }

		/**
         * Lock the mutex and return a guard that will keep it locked until it goes out of scope and
         * allows access to the inner value.
         *
         * @return mutex guard for the inner value
         * @throws std::system_error in case the underlying mutex implementation throws it
         */
        [[nodiscard]] mutex_guard<value_type> lock() {
            return mutex_guard<value_type>{std::unique_lock<mutex_type>{mutex_}, value_};
        }

		/**
         * Attempt to lock the mutex and return a guard that will keep it locked until it goes out of scope and
         * allows access to the inner value.
         *
         * @return nullopt in case the mutex could not be locked, otherwise a mutex guard for the inner value
         * @throws std::system_error in case the underlying mutex implementation throws it
         */
        [[nodiscard]] std::optional<mutex_guard<value_type>> try_lock() {
            std::unique_lock<mutex_type> lock{mutex_, std::try_to_lock};
            if (!lock.owns_lock()) {
                return std::nullopt;
            }

            return mutex_guard<value_type>{std::move(lock), value_};
        }
    };

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_MUTEX_HPP
