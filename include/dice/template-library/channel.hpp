#ifndef DICE_TEMPLATELIBRARY_CHANNEL_HPP
#define DICE_TEMPLATELIBRARY_CHANNEL_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

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
        size_t max_cap_;                       ///< maximum allowed number of elements in queue_
        std::vector<std::optional<T>> queue_;  ///< ring buffer for elements, grows up to max_cap_ and never shrinks
        size_t head_ = 0;                      ///< index of the first element in queue_
        size_t size_ = 0;                      ///< number of elements in queue_

        std::atomic_flag closed_ = ATOMIC_FLAG_INIT;  ///< true if this channel is closed
        std::mutex queue_mutex_;                      ///< mutex for queue_
        std::condition_variable queue_not_empty_;     ///< condvar for size_ > 0
        std::condition_variable queue_not_full_;      ///< condvar for size_ < max_cap_

        template<typename... Args>
        void emplace_back(Args &&...args) {
            if (size_ == queue_.size()) {
                // full, move the elements to a larger buffer
                std::vector<std::optional<T>> grown(std::min(max_cap_, std::max(size_t{1}, 2 * size_)));
                for (size_t ix = 0; ix < size_; ++ix) {
                    grown[ix].emplace(std::move_if_noexcept(*queue_[(head_ + ix) % queue_.size()]));
                }
                queue_ = std::move(grown);
                head_ = 0;
            }

            auto const tail = head_ + size_ < queue_.size() ? head_ + size_ : head_ + size_ - queue_.size();
            queue_[tail].emplace(std::forward<Args>(args)...);
            ++size_;
        }

        std::optional<T> pop_front() noexcept(std::is_nothrow_move_constructible_v<T>) {
            auto ret = std::exchange(queue_[head_], std::nullopt);
            head_ = head_ + 1 < queue_.size() ? head_ + 1 : 0;
            --size_;
            return ret;
        }

    public:
        /**
         * @param capacity maximum number of elements in the channel, memory for them is allocated when needed
         */
        explicit channel(size_t capacity) : max_cap_{capacity} {
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
        template<typename... Args>
        bool emplace(Args &&...args) {
            if (closed_.test(std::memory_order_acquire)) [[unlikely]] {
                return false;
            }

            {
                std::unique_lock lock{queue_mutex_};
                queue_not_full_.wait(lock, [this]() noexcept {
                    return size_ < max_cap_ || closed_.test(std::memory_order_relaxed);
                });

                if (closed_.test(std::memory_order_relaxed)) [[unlikely]] {
                    // relaxed is enough because we hold the lock
                    // wait was exited because closed_ was true (channel closed)
                    return false;
                }

                try {
                    emplace_back(std::forward<Args>(args)...);
                } catch (...) {
                    // pass the wakeup on to another waiting producer
                    queue_not_full_.notify_one();
                    throw;
                }
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
        template<typename... Args>
        bool try_emplace(Args &&...args) {
            if (closed_.test(std::memory_order_acquire)) [[unlikely]] {
                return false;
            }

            {
                std::unique_lock lock{queue_mutex_};
                if (size_ >= max_cap_ || closed_.test(std::memory_order_relaxed)) {
                    // relaxed is enough because we hold the lock
                    return false;
                }

                emplace_back(std::forward<Args>(args)...);
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
        bool push(value_type const &value) {
            return emplace(value);
        }

        /**
         * Push a single element into the channel, returns immediately if there is no capacity in the channel.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool try_push(value_type const &value) {
            return try_emplace(value);
        }

        /**
         * Push a single element into the channel, blocks if there is no capacity left in the channel.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool push(value_type &&value) {
            return emplace(std::move(value));
        }

        /**
         * Push a single element into the channel, returns immediately if there is no capacity in the channel.
         *
         * @param value the element to push
         * @return true if pushing the element succeeded because the channel is not yet closed
         */
        bool try_push(value_type &&value) {
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
            queue_not_empty_.wait(lock, [this]() noexcept {
                return size_ > 0 || closed_.test(std::memory_order_relaxed);
            });

            if (size_ == 0) [[unlikely]] {
                // implies closed_ == true
                return std::nullopt;
            }

            auto ret = pop_front();

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
            if (size_ == 0) {
                return std::nullopt;
            }

            auto ret = pop_front();

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
