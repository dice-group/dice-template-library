#ifndef DICE_TEMPLATELIBRARY_CHANNEL_HPP
#define DICE_TEMPLATELIBRARY_CHANNEL_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace dice::template_library {

    namespace channel_detail {

        /**
         * A ring buffer of `T` in raw storage. Not thread safe.
         *
         * @tparam T value type of the ring buffer
         */
        template<typename T>
        struct ring {
        private:
            T *buf_ = nullptr;  ///< storage for `cap_` elements
            size_t cap_ = 0;    ///< number of elements `buf_` has space for
            size_t head_ = 0;   ///< index of the first element in `buf_`
            size_t size_ = 0;   ///< number of elements in `buf_`

            /**
             * @return index in `buf_` of the element at position `pos` behind `head_`, `pos` is at most `cap_`
             */
            [[nodiscard]] size_t index(size_t pos) const noexcept {
                return head_ + pos < cap_ ? head_ + pos : head_ + pos - cap_;
            }

        public:
            ring() noexcept = default;
            ring(ring const &other) = delete;
            ring(ring &&other) = delete;
            ring &operator=(ring const &other) = delete;
            ring &operator=(ring &&other) = delete;

            ~ring() {
                clear();
            }

            [[nodiscard]] size_t size() const noexcept {
                return size_;
            }

            [[nodiscard]] size_t capacity() const noexcept {
                return cap_;
            }

            /**
             * Construct an element at the back, requires `size() < capacity()`.
             */
            template<typename... Args>
            void emplace_back(Args &&...args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
                assert(size_ < cap_);
                std::construct_at(buf_ + index(size_), std::forward<Args>(args)...);
                ++size_;
            }

            /**
             * Remove the first element, requires `size() > 0`.
             * If moving the element throws, the ring buffer is unchanged.
             */
            [[nodiscard]] std::optional<T> pop_front() noexcept(std::is_nothrow_move_constructible_v<T>) {
                assert(size_ > 0);
                std::optional<T> ret{std::move(buf_[head_])};
                std::destroy_at(buf_ + head_);
                head_ = index(1);
                --size_;
                return ret;
            }

            /**
             * Move the elements to new storage for `new_cap` elements, requires `new_cap >= size()`.
             * If moving an element throws, the ring buffer keeps its storage and its elements.
             * The elements are unchanged if `T` is copyable or nothrow movable.
             */
            void reallocate(size_t new_cap) {
                assert(new_cap >= size_);
                T *new_buf = std::allocator<T>{}.allocate(new_cap);

                size_t moved = 0;
                try {
                    for (; moved < size_; ++moved) {
                        std::construct_at(new_buf + moved, std::move_if_noexcept(buf_[index(moved)]));
                    }
                } catch (...) {
                    std::destroy(new_buf, new_buf + moved);
                    std::allocator<T>{}.deallocate(new_buf, new_cap);
                    throw;
                }

                auto const size = size_;
                clear();
                buf_ = new_buf;
                cap_ = new_cap;
                size_ = size;
            }

            /**
             * Destroy all elements and free the storage.
             */
            void clear() noexcept {
                for (; size_ > 0; --size_) {
                    std::destroy_at(buf_ + head_);
                    head_ = index(1);
                }
                if (buf_ != nullptr) {
                    std::allocator<T>{}.deallocate(buf_, cap_);
                }
                buf_ = nullptr;
                cap_ = 0;
                head_ = 0;
            }
        };

        /**
         * Implementation of `channel` and `fixed_channel`.
         *
         * @tparam T value type of the channel
         * @tparam Fixed if true, the memory for all elements is allocated in the constructor,
         *      otherwise it is allocated when needed and freed after a burst
         */
        template<typename T, bool Fixed>
        struct basic_channel {
            using value_type = T;
            using size_type = size_t;
            using difference_type = std::ptrdiff_t;
            using reference = value_type &;
            using const_reference = value_type const &;
            using pointer = T *;
            using const_pointer = T const *;

        private:
            /**
             * Number of elements a growable channel allocates first and keeps when it is empty.
             * A larger buffer is freed when the channel is empty.
             */
            static constexpr size_t min_buffer_size = std::max(size_t{1}, size_t{64 * 1024} / sizeof(T));

            template<typename... Args>
            static constexpr bool nothrow_emplace = Fixed && std::is_nothrow_constructible_v<T, Args...>;

            size_t max_cap_;  ///< maximum allowed number of elements in `queue_`
            ring<T> queue_;   ///< queue for elements

            std::atomic_flag closed_ = ATOMIC_FLAG_INIT;  ///< true if this channel is closed
            std::mutex queue_mutex_;                      ///< mutex for `queue_`
            std::condition_variable queue_not_empty_;     ///< condvar for `queue_.size() > 0`
            std::condition_variable queue_not_full_;      ///< condvar for `queue_.size() < max_cap_`

            template<typename... Args>
            void emplace_back(Args &&...args) noexcept(nothrow_emplace<Args...>) {
                if constexpr (!Fixed) {
                    if (queue_.size() == queue_.capacity()) {
                        queue_.reallocate(std::min(max_cap_, std::max(min_buffer_size, 2 * queue_.size())));
                    }
                }
                queue_.emplace_back(std::forward<Args>(args)...);
            }

            [[nodiscard]] std::optional<T> pop_front() noexcept(std::is_nothrow_move_constructible_v<T>) {
                auto ret = queue_.pop_front();
                if constexpr (!Fixed) {
                    if (queue_.size() == 0 && queue_.capacity() > min_buffer_size) {
                        queue_.clear();
                    }
                }
                return ret;
            }

        public:
            /**
             * @param capacity maximum number of elements in the channel
             */
            explicit basic_channel(size_t capacity) : max_cap_{capacity} {
                if constexpr (Fixed) {
                    queue_.reallocate(capacity);
                }
            }

            // there is no way to safely implement these with concurrent access
            basic_channel(basic_channel const &other) = delete;
            basic_channel(basic_channel &&other) = delete;
            basic_channel &operator=(basic_channel const &other) = delete;
            basic_channel &operator=(basic_channel &&other) noexcept = delete;

            ~basic_channel() noexcept = default;

            /**
             * Close the channel.
             * After calling close, calls to `push()` return false
             * and calls to `pop()` return `std::nullopt` once the already present elements are exhausted.
             */
            void close() noexcept {
                {
                    // "Even if the shared variable is atomic, it must be modified while owning the mutex to correctly publish the modification to the waiting thread."
                    // - https://en.cppreference.com/w/cpp/thread/condition_variable
                    //
                    // Here closed_ is the shared variable used by queue_not_empty_ in another thread (the one waiting in pop)
                    std::lock_guard lock{queue_mutex_};
                    closed_.test_and_set(std::memory_order_release);
                }
                queue_not_empty_.notify_all();  // notify pop() so that it does not get stuck
                queue_not_full_.notify_all();   // notify emplace()
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
            bool emplace(Args &&...args) noexcept(nothrow_emplace<Args...>) {
                if (closed_.test(std::memory_order_acquire)) [[unlikely]] {
                    return false;
                }

                {
                    std::unique_lock lock{queue_mutex_};
                    queue_not_full_.wait(lock, [this]() noexcept {
                        return queue_.size() < max_cap_ || closed_.test(std::memory_order_relaxed);
                    });

                    if (closed_.test(std::memory_order_relaxed)) [[unlikely]] {
                        // relaxed is enough because we hold the lock
                        // wait was exited because closed_ was true (channel closed)
                        return false;
                    }

                    if constexpr (nothrow_emplace<Args...>) {
                        emplace_back(std::forward<Args>(args)...);
                    } else {
                        try {
                            emplace_back(std::forward<Args>(args)...);
                        } catch (...) {
                            // pass the wakeup on to another waiting producer
                            queue_not_full_.notify_one();
                            throw;
                        }
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
            bool try_emplace(Args &&...args) noexcept(nothrow_emplace<Args...>) {
                if (closed_.test(std::memory_order_acquire)) [[unlikely]] {
                    return false;
                }

                {
                    std::unique_lock lock{queue_mutex_};
                    if (queue_.size() >= max_cap_ || closed_.test(std::memory_order_relaxed)) {
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
            bool push(value_type const &value) noexcept(nothrow_emplace<value_type const &>) {
                return emplace(value);
            }

            /**
             * Push a single element into the channel, returns immediately if there is no capacity in the channel.
             *
             * @param value the element to push
             * @return true if pushing the element succeeded because the channel is not yet closed
             */
            bool try_push(value_type const &value) noexcept(nothrow_emplace<value_type const &>) {
                return try_emplace(value);
            }

            /**
             * Push a single element into the channel, blocks if there is no capacity left in the channel.
             *
             * @param value the element to push
             * @return true if pushing the element succeeded because the channel is not yet closed
             */
            bool push(value_type &&value) noexcept(nothrow_emplace<value_type>) {
                return emplace(std::move(value));
            }

            /**
             * Push a single element into the channel, returns immediately if there is no capacity in the channel.
             *
             * @param value the element to push
             * @return true if pushing the element succeeded because the channel is not yet closed
             */
            bool try_push(value_type &&value) noexcept(nothrow_emplace<value_type>) {
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
                    return queue_.size() > 0 || closed_.test(std::memory_order_relaxed);
                });

                if (queue_.size() == 0) [[unlikely]] {
                    // implies closed_ == true
                    return std::nullopt;
                }

                // initialized by a lambda, because `std::optional<T>` is not assignable if `T` is not
                std::optional<value_type> ret = [this]() {
                    if constexpr (std::is_nothrow_move_constructible_v<value_type>) {
                        return pop_front();
                    } else {
                        try {
                            return pop_front();
                        } catch (...) {
                            // pass the wakeup on to another waiting consumer
                            queue_not_empty_.notify_one();
                            throw;
                        }
                    }
                }();

                lock.unlock();
                queue_not_full_.notify_one();
                return ret;
            }

            /**
             * Try to get a (previously pushed) element from the channel.
             * Unlike `pop()`, if there is no element available, returns `std::nullopt` immediately.
             */
            [[nodiscard]] std::optional<value_type> try_pop() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
                std::unique_lock lock{queue_mutex_};
                if (queue_.size() == 0) {
                    return std::nullopt;
                }

                auto ret = pop_front();

                lock.unlock();
                queue_not_full_.notify_one();
                return ret;
            }

            struct iterator {
                using channel_type = basic_channel;
                using value_type = T;
                using difference_type = std::ptrdiff_t;
                using reference = T &;
                using const_reference = T const &;
                using pointer = typename channel_type::pointer;
                using const_pointer = typename channel_type::const_pointer;
                using iterator_category = std::input_iterator_tag;

            private:
                channel_type *chan_;
                mutable std::optional<value_type> buf_;  ///< this has to be mutable for this iterator to fullfill std::input_iterator

                void advance() noexcept(std::is_nothrow_move_constructible_v<value_type>) {
                    buf_ = chan_->pop();
                }

            public:
                explicit iterator(channel_type *chan) noexcept(std::is_nothrow_move_constructible_v<value_type>)
                    : chan_{chan} {
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

    }  // namespace channel_detail

    /**
     * A multi producer, multi consumer channel/queue
     * @warning close() must be called once the producing threads are done, otherwise the reading thread will hang indefinitely
     *
     * Memory for the elements is allocated when needed, the buffer grows up to the capacity.
     * A buffer larger than 64 KiB is freed when the channel is empty.
     * Use `fixed_channel` if pushing must not allocate.
     *
     * @tparam T value type of the channel
     */
    template<typename T>
    struct channel : channel_detail::basic_channel<T, false> {
        using channel_detail::basic_channel<T, false>::basic_channel;
    };

}  // namespace dice::template_library

#endif  // DICE_TEMPLATELIBRARY_CHANNEL_HPP
