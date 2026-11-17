#ifndef DICE_TEMPLATELIBRARY_BATCHEDSPSCCHANNEL_HPP
#define DICE_TEMPLATELIBRARY_BATCHEDSPSCCHANNEL_HPP

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

namespace dice::template_library {

    /**
     * A single producer, single consumer channel that transfers the elements in batches.
     * The consumer receives the elements in FIFO order, there is one synchronization per batch.
     * Three buffers rotate between producer and consumer, thus there are no allocations after construction.
     *
     * @note the consumer only sees a batch when it is full, or after flush() or close(). This channel is optimized for throughput, NOT latency.
     */
    template<typename T>
    struct batched_spsc_channel {
        using value_type = T;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type &;
        using pointer = T *;

    private:
        static constexpr uint32_t index_mask = 0b11;
        static constexpr uint32_t full_flag = 0b100;   ///< the buffer in slot_ is a batch for the consumer
        static constexpr uint32_t closed_flag = 0b1000;///< the channel is closed

        std::array<std::vector<T>, 3> bufs_;
        uint32_t push_buf_ = 0;         ///< buffer of the producer
        uint32_t pop_buf_ = 1;          ///< buffer of the consumer
        size_t pop_pos_ = 0;            ///< next element in the buffer of the consumer
        std::atomic<uint32_t> slot_ = 2;///< index of the buffer in transit, plus flags

        /// wait while the flags of slot_ are equal to blocked
        uint32_t wait_while(uint32_t blocked) const noexcept {
            auto slot = slot_.load(std::memory_order_acquire);
            while ((slot & ~index_mask) == blocked) {
                slot_.wait(slot, std::memory_order_acquire);
                slot = slot_.load(std::memory_order_acquire);
            }
            return slot;
        }

        /// consumer: if the buffer is exhausted, exchange it for the next batch (stays empty if closed)
        void fill() noexcept {
            auto &buf = bufs_[pop_buf_];
            if (pop_pos_ < buf.size()) {
                return;
            }

            buf.clear();
            pop_pos_ = 0;

            // CAS, because the producer can set closed_flag concurrently
            auto slot = wait_while(0);
            while ((slot & full_flag) && !slot_.compare_exchange_weak(slot, pop_buf_ | (slot & closed_flag),
                                                                      std::memory_order_acq_rel, std::memory_order_acquire)) {
            }

            if (slot & full_flag) {
                pop_buf_ = slot & index_mask;
                slot_.notify_one();
            }
        }

    public:
        /**
         * @param batch_size number of elements that the producer collects before the consumer receives them
         */
        explicit batched_spsc_channel(size_t batch_size = 1024) {
            assert(batch_size > 0);
            for (auto &buf : bufs_) {
                buf.reserve(batch_size);
            }
        }

        // there is no way to safely implement these with concurrent access
        batched_spsc_channel(batched_spsc_channel const &other) = delete;
        batched_spsc_channel(batched_spsc_channel &&other) = delete;
        batched_spsc_channel &operator=(batched_spsc_channel const &other) = delete;
        batched_spsc_channel &operator=(batched_spsc_channel &&other) = delete;

        ~batched_spsc_channel() noexcept = default;

        /**
         * Producer: flush, then close the channel.
         * The consumer can still receive the elements that are already flushed.
         */
        void close() noexcept {
            flush();
            cancel();
        }

        /**
         * Consumer (or producer): close the channel without a flush.
         * After this, the producer drops its elements and flush() returns false.
         */
        void cancel() noexcept {
            slot_.fetch_or(closed_flag, std::memory_order_release);
            slot_.notify_one();
        }

        /**
         * @return true if this channel is closed
         */
        [[nodiscard]] bool closed() const noexcept {
            return (slot_.load(std::memory_order_acquire) & closed_flag) != 0;
        }

        /**
         * Producer: give the current batch to the consumer.
         * Blocks while the consumer did not yet take the previous batch.
         *
         * @return false if the channel is closed, the elements of the batch are then dropped
         */
        bool flush() noexcept {
            auto &buf = bufs_[push_buf_];
            if (buf.empty()) {
                return !closed();
            }

            // CAS, because the consumer can set closed_flag concurrently
            auto slot = wait_while(full_flag);
            if ((slot & closed_flag) || !slot_.compare_exchange_strong(slot, push_buf_ | full_flag,
                                                                      std::memory_order_acq_rel, std::memory_order_acquire)) {
                buf.clear();
                return false;
            }

            push_buf_ = slot;
            slot_.notify_one();
            return true;
        }

        /**
         * Producer: emplace an element into the current batch, flushes if the batch is full.
         *
         * @return false if the flush failed because the channel is closed
         * @note a close is only detected at a flush
         */
        template<typename ...Args>
        bool emplace(Args &&...args) {
            auto &buf = bufs_[push_buf_];
            buf.emplace_back(std::forward<Args>(args)...);
            return buf.size() < buf.capacity() || flush();
        }

        /// @see emplace
        bool push(value_type const &value) {
            return emplace(value);
        }

        /// @see emplace
        bool push(value_type &&value) {
            return emplace(std::move(value));
        }

        using sentinel = std::default_sentinel_t;

        struct iterator {
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using reference = T &;
            using pointer = T *;
            using iterator_category = std::input_iterator_tag;

        private:
            batched_spsc_channel *chan_;

        public:
            explicit iterator(batched_spsc_channel *chan) noexcept : chan_{chan} {
                chan_->fill();
            }

            reference operator*() const noexcept {
                return chan_->bufs_[chan_->pop_buf_][chan_->pop_pos_];
            }

            pointer operator->() const noexcept {
                return &**this;
            }

            iterator &operator++() noexcept {
                ++chan_->pop_pos_;
                chan_->fill();
                return *this;
            }

            void operator++(int) noexcept {
                ++*this;
            }

            bool operator==(sentinel) const noexcept {
                return chan_->pop_pos_ == chan_->bufs_[chan_->pop_buf_].size();
            }
        };

        /**
         * Consumer: an iterator over all present and future elements of this channel
         * @note iterator == end() is true once the channel is closed and all flushed elements are consumed
         */
        [[nodiscard]] iterator begin() noexcept {
            return iterator{this};
        }

        [[nodiscard]] sentinel end() const noexcept {
            return std::default_sentinel;
        }
    };

}// namespace dice::template_library

#endif// DICE_TEMPLATELIBRARY_BATCHEDSPSCCHANNEL_HPP
