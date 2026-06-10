#ifndef DICE_TEMPLATELIBRARY_EXCHANGECHANNEL_HPP
#define DICE_TEMPLATELIBRARY_EXCHANGECHANNEL_HPP

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <iterator>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	/**
     * A multi producer, multi consumer channel
     * that only retains the last sent value.
     */
    template<typename T>
    struct exchange_channel {
        using value_type = T;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type &;
        using const_reference = value_type const &;
        using pointer = T *;
        using const_pointer = T const *;

    private:
        std::mutex value_mutex_; ///< mutex for value_
        std::optional<T> value_; ///< the last value that was sent

        std::atomic_flag closed_ = ATOMIC_FLAG_INIT; ///< true if this channel is closed
        std::condition_variable has_value_; ///< condvar for value_.has_value()

    public:
        exchange_channel() = default;

        // there is no way to safely implement these with concurrent access
        exchange_channel(exchange_channel const &other) = delete;
        exchange_channel(exchange_channel &&other) = delete;
        exchange_channel &operator=(exchange_channel const &other) = delete;
        exchange_channel &operator=(exchange_channel &&other) noexcept = delete;

        ~exchange_channel() noexcept = default;

		/**
         * Close the channel.
         * After calling close calls to push() will return false
         * and calls to try_pop will return std::nullopt once the already present elements are exhausted
         */
        void close() noexcept {
            {
                // "Even if the shared variable is atomic, it must be modified while owning the mutex to correctly publish the modification to the waiting thread."
                // - https://en.cppreference.com/w/cpp/thread/condition_variable
                //
                // Here closed_ is the shared variable used by has_value_ in another thread (the one waiting in pop)
                std::lock_guard lock{value_mutex_};
                closed_.test_and_set(std::memory_order_release);
            }
            has_value_.notify_all(); // notify pop() so that it does not get stuck
        }

		/**
         * @return true if this channel is closed
         */
        [[nodiscard]] bool closed() const noexcept {
            return closed_.test(std::memory_order_acquire);
        }

		/**
         * Emplace an element into the channel, replaces the current element in the channel if there is one.
         *
         * @param args constructor args
         * @return true if emplacing the element succeeded because the channel is not yet closed
         */
        template<typename ...Args>
        bool emplace(Args &&...args) noexcept(std::is_nothrow_constructible_v<value_type, decltype(std::forward<Args>(args))...>) {
            if (closed_.test(std::memory_order_acquire)) [[unlikely]] {
                return false;
            }

            {
                std::unique_lock lock{value_mutex_};
                if (closed_.test(std::memory_order_relaxed)) [[unlikely]] {
                    // relaxed is enough because we hold the lock
                    return false;
                }

                value_.emplace(std::forward<Args>(args)...);
            }

            has_value_.notify_one();
            return true;
        }

		/**
         * Push a single element into the channel, replaces the current element in the channel if there is one.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool push(value_type const &value) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
            return emplace(value);
        }

        /**
         * Push a single element into the channel, replaces the current element in the channel if there is one.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool push(value_type &&value) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            return emplace(std::move(value));
        }

		/**
         * Try to get a (previously pushed) element from the channel.
         * If there is no element available, blocks until there is one available or the channel is closed.
         *
         * @return std::nullopt if the channel was closed, an element otherwise
         */
        [[nodiscard]] std::optional<value_type> pop() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            std::unique_lock lock{value_mutex_};
            has_value_.wait(lock, [this]() noexcept { return value_.has_value() || closed_.test(std::memory_order_relaxed); });

            if (!value_.has_value()) [[unlikely]] {
                // implies closed_ == true
                return std::nullopt;
            }

            return *std::exchange(value_, std::nullopt);
        }

        /**
         * Try to get a (previously pushed) element from the channel.
         * Unlike pop(), if there is no element available, returns std::nullopt immediatly.
         */
        [[nodiscard]] std::optional<value_type> try_pop() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            std::unique_lock lock{value_mutex_};
            if (!value_.has_value()) {
                return std::nullopt;
            }

            return *std::exchange(value_, std::nullopt);
        }

        struct iterator {
            using channel_type = exchange_channel;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using reference = T &;
            using const_reference = T const &;
            using pointer = typename exchange_channel::pointer;
            using const_pointer = typename exchange_channel::const_pointer;
            using iterator_category = std::input_iterator_tag;

        private:
            exchange_channel *chan_;
            mutable std::optional<value_type> buf_; ///< this has to be mutable for this iterator to fullfill std::input_iterator

            void advance() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
                buf_ = chan_->pop();
            }

        public:
            explicit iterator(channel_type *chan) noexcept(std::is_nothrow_move_constructible_v<value_type>) : chan_{chan} {
                advance();
            }

            iterator(iterator const &other) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
                : chan_{other.chan_},
                  buf_{other.buf_} {
            }

            iterator &operator=(iterator const &other) noexcept(std::is_nothrow_copy_assignable_v<value_type>) {
                if (this == &other) {
                    return *this;
                }

                chan_ = other.chan_;
                buf_ = other.buf_;
                return *this;
            }

            iterator(iterator &&other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
                : chan_{other.chan_},
                  buf_{std::move(other.buf_)} {
            }

            iterator &operator=(iterator &&other) noexcept(std::is_nothrow_swappable_v<value_type>) {
                assert(this != &other);
                std::swap(chan_, other.chan_);
                std::swap(buf_, other.buf_);
                return *this;
            }

            /// intentionally returns a reference instead of a const_reference to be able to extract it
            reference operator*() noexcept {
                return *buf_;
            }

            reference operator*() const noexcept {
                return *buf_;
            }

            /// intentionally returns a pointer instead of a const_pointer to be able to extract it
            pointer operator->() noexcept {
                return &*buf_;
            }

            pointer operator->() const noexcept {
                return &*buf_;
            }

            iterator &operator++() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
                advance();
                return *this;
            }

            void operator++(int) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
                advance();
            }

            bool operator==(std::default_sentinel_t) const noexcept {
                return !buf_.has_value();
            }
        };

        using sentinel = std::default_sentinel_t;

		/**
         * @return an iterator over all present and future elements of this channel
         * @note iterator == end() is true once the channel is closed
         */
        [[nodiscard]] iterator begin() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            return iterator{this};
        }

        [[nodiscard]] sentinel end() const noexcept {
            return std::default_sentinel;
        }
    };

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_EXCHANGECHANNEL_HPP
