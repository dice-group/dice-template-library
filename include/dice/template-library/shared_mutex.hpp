#ifndef DICE_TEMPLATELIBRARY_SHAREDMUTEX_HPP
#define DICE_TEMPLATELIBRARY_SHAREDMUTEX_HPP

#include <mutex>
#include <shared_mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	/**
     * An RAII guard for a value behind a shared_mutex.
     * When this shared_mutex_guard is dropped the lock is automatically released.
     *
     * @note to use this correctly it is important to understand that unlike rust, C++ cannot
     *       enforce that you do not use pointers given out by this type beyond its lifetime.
     *       You may not store pointers or references given out via this wrapper beyond its lifetime, otherwise behaviour is undefined.
     *
     * @tparam T the value type protected by the mutex
     * @tparam Mutex the mutex type
     */
    template<typename T, typename Mutex = std::shared_mutex>
    struct shared_mutex_guard {
        using value_type = T;
        using mutex_type = Mutex;
        using lock_type = std::conditional_t<std::is_const_v<T>, std::shared_lock<mutex_type>, std::unique_lock<mutex_type>>;

    private:
        value_type *value_ptr_;
        lock_type lock_;

    public:
        shared_mutex_guard(lock_type &&lock, T &value) noexcept
            : value_ptr_{&value},
              lock_{std::move(lock)} {
        }

        shared_mutex_guard() = delete;
        shared_mutex_guard(shared_mutex_guard const &other) noexcept = delete;
        shared_mutex_guard &operator=(shared_mutex_guard const &other) noexcept = delete;
        shared_mutex_guard(shared_mutex_guard &&other) noexcept = default;
        shared_mutex_guard &operator=(shared_mutex_guard &&other) noexcept = default;
        ~shared_mutex_guard() = default;

        value_type *operator->() const noexcept {
            return value_ptr_;
        }

        value_type &operator*() const noexcept {
            return *value_ptr_;
        }

        friend void swap(shared_mutex_guard const &lhs, shared_mutex_guard const &rhs) noexcept {
            using std::swap;
            swap(lhs.value_ptr_, rhs.value_ptr_);
            swap(lhs.lock_, rhs.lock_);
        }
    };

	/**
     * A rust-like shared_mutex type (https://doc.rust-lang.org/std/sync/struct.RwLock.html) that holds its data instead of living next to it.
     *
     * @note Because this is C++ it cannot be fully safe, like the rust version is, for more details see doc comment on mutex_guard.
     * @note This type is non-movable and non-copyable, if you need to do either of these things use `std::unique_ptr<shared_mutex<T>>` or `std::shared_ptr<shared_mutex<T>>`
     *
     * @tparam T value type stored
     * @tparam Mutex the mutex type
     */
    template<typename T, typename Mutex = std::shared_mutex>
    struct shared_mutex {
        using value_type = T;
        using mutex_type = Mutex;

    private:
        value_type value_;
        mutex_type mutex_;

    public:
        constexpr shared_mutex() noexcept(std::is_nothrow_default_constructible_v<value_type>) = default;
        constexpr ~shared_mutex() noexcept(std::is_nothrow_destructible_v<value_type>) = default;

        shared_mutex(shared_mutex const &other) = delete;
        shared_mutex(shared_mutex &&other) = delete;
        shared_mutex &operator=(shared_mutex const &other) = delete;
        shared_mutex &operator=(shared_mutex &&other) = delete;

        explicit constexpr shared_mutex(value_type const &value) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
            : value_{value} {
        }

        explicit constexpr shared_mutex(value_type &&value) noexcept(std::is_nothrow_move_constructible_v<value_type>)
            : value_{std::move(value)} {
        }

        template<typename ...Args>
        explicit constexpr shared_mutex(std::in_place_t, Args &&...args) noexcept(std::is_nothrow_constructible_v<value_type, decltype(std::forward<Args>(args))...>)
            : value_{std::forward<Args>(args)...} {
        }

		/**
         * Lock the mutex and return a guard that will keep it locked until it goes out of scope and
         * allows access to the inner value.
         *
         * @return mutex guard for the inner value
         * @throws std::system_error in case the underlying mutex implementation throws it
         */
        [[nodiscard]] shared_mutex_guard<value_type> lock() {
            return shared_mutex_guard<value_type>{std::unique_lock<mutex_type>{mutex_}, value_};
        }

		/**
         * Attempt to lock the mutex and return a guard that will keep it locked until it goes out of scope and
         * allows access to the inner value.
         *
         * @return nullopt in case the mutex could not be locked, otherwise a mutex guard for the inner value
         * @throws std::system_error in case the underlying mutex implementation throws it
         */
        [[nodiscard]] std::optional<shared_mutex_guard<value_type>> try_lock() {
            std::unique_lock<mutex_type> lock{mutex_, std::try_to_lock};
            if (!lock.owns_lock()) {
                return std::nullopt;
            }

            return shared_mutex_guard<value_type>{std::move(lock), value_};
        }

        /**
         * Lock the mutex for shared ownership and return a guard that will keep it locked until it goes out of scope and
         * allows access to the inner value.
         *
         * @return mutex guard for the inner value
         * @throws std::system_error in case the underlying mutex implementation throws it
         */
        [[nodiscard]] shared_mutex_guard<value_type const> lock_shared() {
            return shared_mutex_guard<value_type const>{std::shared_lock<mutex_type>{mutex_}, value_};
        }

        /**
         * Attempt to lock the mutex for shared ownership and return a guard that will keep it locked until it goes out of scope and
         * allows access to the inner value.
         *
         * @return nullopt in case the mutex could not be locked, otherwise a mutex guard for the inner value
         * @throws std::system_error in case the underlying mutex implementation throws it
         */
        [[nodiscard]] std::optional<shared_mutex_guard<value_type const>> try_lock_shared() {
            std::shared_lock<mutex_type> lock{mutex_, std::try_to_lock};
            if (!lock.owns_lock()) {
                return std::nullopt;
            }

            return shared_mutex_guard<value_type const>{std::move(lock), value_};
        }
    };

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_SHAREDMUTEX_HPP
