#ifndef DICE_TEMPLATELIBRARY_CHANNEL_HPP
#define DICE_TEMPLATELIBRARY_CHANNEL_HPP

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

namespace dice::template_library {

	/**
     * A multi producer, multi consumer channel/queue
     * @warning close() must be called once the producing threads are done, otherwise the reading thread will hang indefinitely
     *
     * @tparam T value type of the channel
     */
    template<typename T>
    struct channel {
        using value_type = T;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type &;
        using const_reference = value_type const &;
        using pointer = T *;
        using const_pointer = T const *;

    private:
        size_t max_cap_; ///< maximum allowed number of elements in queue_
        std::deque<T> queue_; ///< queue for elements

        std::atomic_flag closed_ = ATOMIC_FLAG_INIT; ///< true if this channel is closed
        std::mutex queue_mutex_; ///< mutex for queue_
        std::condition_variable queue_not_empty_; ///< condvar for queue_.size() > 0
        std::condition_variable queue_not_full_;  ///< condvar for queue_.size() < max_cap_;

    public:
        explicit channel(size_t capacity) : max_cap_{capacity} {
            // note: for some reason std::deque does not have .reserve()
            // TODO: it would probably make sense to implement a VecDeque for this (https://doc.rust-lang.org/std/collections/struct.VecDeque.html)
        }

        // there is no way to safely implement these with concurrent access
        channel(channel const &other) = delete;
        channel(channel &&other) = delete;
        channel &operator=(channel const &other) = delete;
        channel &operator=(channel &&other) noexcept = delete;

        ~channel() noexcept = default;

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
                // Here closed_ is the shared variable used by queue_not_empty_ in another thread (the one waiting in try_pop)
                std::lock_guard lock{queue_mutex_};
                closed_.test_and_set(std::memory_order_release);
            }
            queue_not_empty_.notify_all(); // notify pop() so that it does not get stuck
            queue_not_full_.notify_all(); // notify emplace()
        }

		/**
         * @return true if this channel is closed
         */
        [[nodiscard]] bool closed() const noexcept {
            return closed_.test(std::memory_order_acquire);
        }

		/**
         * Emplace an element into the channel, blocks if there is no capacity left in the channel.
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
                std::unique_lock lock{queue_mutex_};
                queue_not_full_.wait(lock, [this]() noexcept { return queue_.size() < max_cap_ || closed_.test(std::memory_order_relaxed); });

                if (closed_.test(std::memory_order_relaxed)) [[unlikely]] {
                    // relaxed is enough because we hold the lock
                    // wait was exited because closed_ was true (channel closed)
                    return false;
                }

                queue_.emplace_back(std::forward<Args>(args)...);
            }

            queue_not_empty_.notify_one();
            return true;
        }

        /**
         * Emplace an element into the channel, returns immediately if there is no capacity in the channel.
         *
         * @param args constructor args
         * @return true if emplacing the element succeeded
         */
        template<typename ...Args>
        bool try_emplace(Args &&...args) noexcept(std::is_nothrow_constructible_v<value_type, decltype(std::forward<Args>(args))...>) {
            if (closed_.test(std::memory_order_acquire)) [[unlikely]] {
                return false;
            }

            {
                std::unique_lock lock{queue_mutex_};
                if (queue_.size() >= max_cap_ || closed_.test(std::memory_order_relaxed)) {
                    // relaxed is enough because we hold the lock
                    return false;
                }

                queue_.emplace_back(std::forward<Args>(args)...);
            }

            queue_not_empty_.notify_one();
            return true;
        }

		/**
         * Push a single element into the channel, blocks if there is no capacity left in the channel.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool push(value_type const &value) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
            return emplace(value);
        }

        /**
         * Push a single element into the channel, returns immediately if there is no capacity in the channel.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool try_push(value_type const &value) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
            return try_emplace(value);
        }

        /**
         * Push a single element into the channel, blocks if there is no capacity left in the channel.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool push(value_type &&value) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            return emplace(std::move(value));
        }

        /**
         * Push a single element into the channel, returns immediately if there is no capacity in the channel.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool try_push(value_type &&value) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            return try_emplace(std::move(value));
        }

		/**
         * Try to get a (previously pushed) element from the channel.
         * If there is no element available, blocks until there is one available or the channel is closed.
         *
         * @return std::nullopt if the channel was closed, an element otherwise
         */
        [[nodiscard]] std::optional<value_type> pop() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            std::unique_lock lock{queue_mutex_};
            queue_not_empty_.wait(lock, [this]() noexcept { return !queue_.empty() || closed_.test(std::memory_order_relaxed); });

            if (queue_.empty()) [[unlikely]] {
                // implies closed_ == true
                return std::nullopt;
            }

            auto ret = std::move(queue_.front());
            queue_.pop_front();

			lock.unlock();
            queue_not_full_.notify_one();
            return ret;
        }

        /**
         * Try to get a (previously pushed) element from the channel.
         * Unlike pop(), if there is no element available, returns std::nullopt immediatly.
         */
        [[nodiscard]] std::optional<value_type> try_pop() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
            std::unique_lock lock{queue_mutex_};
            if (queue_.empty()) {
                return std::nullopt;
            }

            auto ret = std::move(queue_.front());
            queue_.pop_front();

            lock.unlock();
            queue_not_full_.notify_one();
            return ret;
        }

        struct iterator {
            using channel_type = channel;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using reference = T &;
            using const_reference = T const &;
            using pointer = typename channel_type::pointer;
            using const_pointer = typename channel_type::const_pointer;
            using iterator_category = std::input_iterator_tag;

        private:
            channel_type *chan_;
            mutable std::optional<value_type> buf_; ///< this has to be mutable for this iterator to fullfill std::input_iterator

            void advance() noexcept {
                buf_ = chan_->pop();
            }

        public:
            explicit iterator(channel_type *chan) noexcept : chan_{chan} {
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

            reference operator*() noexcept {
                return *buf_;
            }

            reference operator*() const noexcept {
                return *buf_;
            }

            pointer operator->() noexcept {
                return &*buf_;
            }

            pointer operator->() const noexcept {
                return &*buf_;
            }

            iterator &operator++() noexcept {
                advance();
                return *this;
            }

            void operator++(int) noexcept {
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
        [[nodiscard]] iterator begin() noexcept {
            return iterator{this};
        }

        [[nodiscard]] sentinel end() const noexcept {
            return std::default_sentinel;
        }
    };

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_CHANNEL_HPP
