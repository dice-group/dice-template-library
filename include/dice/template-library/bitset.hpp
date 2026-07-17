#ifndef DICE_TEMPLATELIBRARY_BITSET_HPP
#define DICE_TEMPLATELIBRARY_BITSET_HPP

#include <dice/template-library/flex_array.hpp>
#include <bit>
#include <functional>
#include <numeric>
#include <format>

namespace dice::template_library {

    struct bit_and_op{};
    struct add_op{};
    struct bit_or_op{};

    template<typename Tp>
    struct merge_functor {
        Tp operator()(add_op, Tp const v1, Tp const v2) const noexcept {
            return v1 + v2;
        };

        Tp operator()(bit_and_op, Tp const v1, Tp const v2) const noexcept {
            return v1 & v2;
        };

        Tp operator()(bit_or_op, Tp const v1, Tp const v2) const noexcept {
            return v1 | v2;
        };
    };

    struct bitset_const {
        static constexpr size_t bit_mode = 0x00;
        static constexpr size_t segment_mode = 0x01;
    };

    template<typename T, size_t extent, size_t segments>
    struct bitset {
        static constexpr bool   has_storage_limit = segments != dynamic_extent;
        static constexpr size_t segment_size = sizeof(T);
        static constexpr size_t segment_align = alignof(T);
        static constexpr size_t segment_steps = segment_size / segment_align; ///> how many chunks fit within one segment
        static constexpr size_t segment_size_in_bits = segment_size * 8;
        static constexpr size_t storage_size = !has_storage_limit ? dynamic_extent : segment_size * segments;
        static constexpr size_t storage_size_in_bits = !has_storage_limit ? dynamic_extent : storage_size * 8;

        using storage   = flex_array<T, extent, segments>;
        using global_ix = size_t;
        using segment   = size_t;
        using offset    = size_t;

        using storage_word = std::conditional_t<segment_align >= alignof(std::uint64_t), std::uint64_t,
            std::conditional_t<segment_align >= alignof(std::uint32_t), std::uint32_t,
            std::conditional_t<segment_align >= alignof(std::uint16_t), std::uint16_t, std::uint8_t>>>;
        using storage_word_pointer = storage_word *;
        using storage_word_const_pointer = storage_word const *;

    private:
        struct bitset_iterator {
        private:
            segment       cur_segment_{};
            offset        cur_offset_{};
            bitset *const backing_bitset_;

        public:
            explicit bitset_iterator(bitset &bitset) noexcept :
                backing_bitset_{&bitset} {}

            explicit bitset_iterator(bitset &bitset, offset const& o) :
                backing_bitset_{&bitset}{
                if (o >= segment_size_in_bits) {
                    throw std::out_of_range{"bitset_iterator: o >= segment_size"};
                }
                cur_offset_ = o;
            }

            explicit bitset_iterator(bitset &bitset, offset const& o, segment const& s) :
                backing_bitset_{&bitset}{
                if (o >= segment_size_in_bits) {
                    throw std::out_of_range{"bitset_iterator: o >= segment_size"};
                }

                if (s >= bitset.size()) {
                    throw std::out_of_range{"bitset_iterator: segment out of bounds"};
                }

                cur_offset_ = o;
                cur_segment_ = s;
            }

            void operator=(bool const b) const noexcept {
                if (b) {
                    backing_bitset_->set(calc_global_idx(cur_segment_, cur_offset_));
                    return;
                }
                backing_bitset_->unset(calc_global_idx(cur_segment_, cur_offset_));
            }

            bool operator*() const noexcept {
                return backing_bitset_->test(calc_global_idx(cur_segment_, cur_offset_));
            }

            // shared iterator for mode=0 (bits) mode>=1 (segments)
            template<size_t mode=bitset_const::bit_mode>
            bitset_iterator& operator++() noexcept {
                if constexpr (mode == bitset_const::bit_mode) {
                    if (++cur_offset_ >= segment_size_in_bits) {
                        ++cur_segment_;
                        cur_offset_ = 0;
                        return *this;
                    }
                    return *this;
                }
                ++cur_segment_;
                return *this;
            }

            template<size_t mode=bitset_const::bit_mode>
            bitset_iterator operator++(int) noexcept {
                auto tmp = *this;
                operator++<mode>();
                return tmp;
            }

            template<size_t mode=bitset_const::bit_mode>
            bitset_iterator& operator+=(size_t const skip) noexcept {
                auto skip_handler = [this](size_t const skip_size) {
                    auto global_ix = calc_global_idx(cur_segment_, cur_offset_) + skip_size;

                    if (global_ix >= storage_size_in_bits) {
                        cur_segment_ = segments;
                        cur_offset_ = 0;
                        return;
                    }

                    auto offset = calc_which_offset(global_ix);
                    auto seg = calc_which_segment(global_ix);

                    cur_segment_ = seg;
                    cur_offset_ = offset;
                };

                if constexpr (mode == bitset_const::bit_mode) {
                    skip_handler(skip);
                }
                else {
                    skip_handler(skip * segment_size_in_bits);
                }

                return *this;
            }

            bitset_iterator operator+(size_t rh_add) const noexcept {
                bitset_iterator tmp = *this;
                tmp += rh_add;
                return tmp;
            }

            T const& get() const {
                return *(backing_bitset_->inner_.data() + cur_segment_);
            }

            T& get() {
                return *(backing_bitset_->inner_.data() + cur_segment_);
            }

            bool consumed() const noexcept {
                return cur_segment_ >= backing_bitset_->size();
            }

            friend bool operator==(std::default_sentinel_t, bitset_iterator const& it) {
                return it == std::default_sentinel;
            }

            friend bool operator==(bitset_iterator const& it, std::default_sentinel_t) {
                return it.consumed();
            }
        };

        struct AutoModeTag {};
        struct DefaultModeTag {};

        struct Set{};
        struct Unset{};

        storage inner_;

        [[nodiscard]] constexpr size_t require_segments(global_ix const ix) const requires (!has_storage_limit) {
            auto const bit_pos = bits_consumed();
            if (bit_pos >= ix) {
                return 0;
            }

            auto const bit_off = ix - bit_pos;

            return [bit_off] {
                if constexpr (std::has_single_bit(segment_size_in_bits)) {
                    return (bit_off + segment_size_in_bits - 1) >> std::countr_zero(segment_size_in_bits);
                }
                else {
                    return (bit_off + segment_size_in_bits - 1) / segment_size_in_bits;
                }
            }();
        }

        void constexpr expand_segments(global_ix const ix) requires (!has_storage_limit) {
            auto const to_add = require_segments(ix);
            if (to_add == 0) {
                return;
            }

            inner_.resize(inner_.size() + to_add);
        }

        [[nodiscard]] constexpr size_t bits_consumed() const noexcept {
            return inner_.size() * segment_size_in_bits;
        }

        [[nodiscard]] static constexpr bool fits_in_storage(global_ix const ix) noexcept {
            if constexpr (has_storage_limit) {
                return ix < storage_size_in_bits;
            }

            return true;
        }

        [[nodiscard]] static constexpr segment calc_which_segment(global_ix const ix) noexcept {
            if constexpr (std::has_single_bit(segment_size_in_bits)) {
                return ix >> std::countr_zero(segment_size_in_bits);
            }
            else {
                return ix / segment_size_in_bits;
            }
        }

        [[nodiscard]] static constexpr offset calc_which_offset(global_ix const ix) noexcept {
            if constexpr (std::has_single_bit(segment_size_in_bits)) {
                return ix & (segment_size_in_bits-1);
            }
            else {
                return ix % segment_size_in_bits;
            }
        }

        [[nodiscard]] static constexpr global_ix calc_global_idx(segment const s, offset const o) noexcept {
            return s * segment_size_in_bits + o;
        }

        template<typename F>
        auto bitset_mod_cntl(AutoModeTag, F&& ops, global_ix const ix) -> std::invoke_result_t<F, bitset*, size_t, size_t> {
            if (!fits_in_storage(ix)) {
                throw std::out_of_range{"bitset::set: ix out of range"};
            }

            // auto expands if we don't have storage limit
            if constexpr (!has_storage_limit) {
                expand_segments(ix);
            }

            auto const segment = calc_which_segment(ix);
            auto const offset  = calc_which_offset(ix);

            using result_t = std::invoke_result_t<F, bitset*, size_t, size_t>;

            if constexpr (std::is_void_v<result_t>) {
                std::invoke(std::forward<F>(ops), this, segment, offset);
                return;
            }
            else {
                return std::invoke(std::forward<F>(ops), this, segment, offset);
            }
        }

        template<typename F>
        auto bitset_mod_cntl(DefaultModeTag, F&& ops, global_ix const ix) -> std::invoke_result_t<F, bitset*, size_t, size_t> {
            if (!fits_in_storage(ix)) {
                throw std::out_of_range{"bitset::set: ix out of range"};
            }

            auto const segment = calc_which_segment(ix);
            auto const offset  = calc_which_offset(ix);

            if constexpr (std::is_void_v<std::invoke_result_t<F, bitset*, size_t, size_t>>) {
                std::invoke(std::forward<F>(ops), this, segment, offset);
                return;
            }
            else {
                return std::invoke(std::forward<F>(ops), this, segment, offset);
            }
        }

        template<typename F>
        auto bitset_op_cntl(F &&ops, storage::const_reference segment) const -> std::invoke_result_t<F, bitset*, typename storage::const_reference> {
            if constexpr (std::is_void_v<std::invoke_result_t<F, bitset*, typename storage::const_reference>>) {
                std::invoke(std::forward<F>(ops), this, segment);
                return;
            }
            else {
                return std::invoke(std::forward<F>(ops), this, segment);
            }
        }

        [[nodiscard]] static size_t offset_in_chunk(offset const o) noexcept {
            return o % segment_align * 8;
        }

        [[nodiscard]] static size_t which_chunk(offset const o) noexcept {
            return o / segment_align * 8;
        }

        storage_word_pointer get_chunk_raw(segment const s, offset const o) const noexcept {
            return reinterpret_cast<storage_word_pointer>(inner_.data() + s) + which_chunk(o);
        }

        void segment_set(segment const s, offset const o) noexcept {
            if constexpr (std::is_integral_v<T>) {
                *(inner_.data() + s) |= 1uz << o;
            }
            else {
                auto chunk_raw = get_chunk_raw(s, o);
                *chunk_raw |= 1uz << offset_in_chunk(o);
            }
        }

        void segment_flip(segment const s, offset const o) noexcept {
            if constexpr (std::is_integral_v<T>) {
                *(inner_.data() + s) ^= 1uz << o;
            }
            else {
                auto chunk_raw = get_chunk_raw(s, o);
                *chunk_raw ^= 1uz << offset_in_chunk(o);
            }
        }

        void segment_unset(segment const s, offset const o) noexcept {
            if constexpr (std::is_integral_v<T>) {
                *(inner_.data() + s) &= ~(1uz << o);
            }
            else {
                auto chunk_raw = get_chunk_raw(s, o);
                *chunk_raw &= ~(1uz << offset_in_chunk(o));
            }
        }

        [[nodiscard]] bool segment_test(segment const s, offset const o) noexcept {
            if constexpr (std::is_integral_v<T>) {
                return *(inner_.data() + s) & 1uz << o;
            }
            else {
                auto chunk_raw = get_chunk_raw(s, o);
                return *chunk_raw & 1uz << offset_in_chunk(o);
            }
        }

        [[nodiscard]] size_t segment_count(storage::const_reference segment) const noexcept {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::popcount(segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                return std::popcount(word);
            }, merge_functor<storage_word>{}, 0uz);
        }

#ifdef __SIZEOF_INT128__
        [[nodiscard]] size_t segment_count(__uint128_t const& segment) const noexcept {
            uint64_t const lo = static_cast<uint64_t>(segment);
            uint64_t const hi = static_cast<uint64_t>(segment >> 64);

            return std::popcount(lo) + std::popcount(hi);
        }
#endif

        template<typename F, typename M, typename Tp, typename Ops>
        requires std::is_same_v<std::invoke_result_t<F, typename storage::const_reference>, Tp> &&
            std::invocable<M, Ops, Tp, Tp>
        [[nodiscard]] Tp segment_handler(F &&handler, M &&merge, Tp initial, Ops ops=add_op{}) const {
            Tp merge_val{initial};
            for (auto const &segment : inner_) {
                merge_val = std::invoke(std::forward<M>(merge), ops, merge_val,
                    std::invoke(std::forward<F>(handler), segment));
            }

            return merge_val;
        }

        template<typename F>
        bool segment_handler(F &&handler, bitset const& other) const {
            auto self_it = begin();
            auto outer_it = other.begin();

            if (size() != other.size()) {
                return false;
            }

            auto end_sentinel = end();

            while (self_it != end_sentinel) {
                if (!std::invoke(std::forward<F>(handler), self_it.get(), outer_it.get())) {
                    return false;
                }
                ++self_it;
                ++outer_it;
            }
            return true;
        }

        template<typename F, typename Pr>
        bool segment_handler(F &&handler, Pr &&pred) const {
            auto self_it = begin();
            auto end_sentinel = end();

            while (self_it != end_sentinel) {
                if (auto const val = std::invoke(std::forward<F>(handler), self_it.get()); !std::invoke(std::forward<Pr>(pred), val)) {
                    return false;
                }
                ++self_it;
            }
            return true;
        }

        template<typename F, typename Pr, typename M, typename Tp, typename Ops>
        Tp segment_handler(F &&handler, Pr &&pred, M &&merge, Tp initial, Ops ops=add_op{}) const {
            auto self_it = begin();
            auto end_sentinel = end();

            Tp merge_val{initial};

            while (self_it != end_sentinel) {
                Tp const val = std::invoke(std::forward<F>(handler), self_it.get());
                merge_val = std::invoke(std::forward<M>(merge), ops, merge_val, val);
                if (!std::invoke(std::forward<Pr>(pred), val)) {
                    return merge_val;
                }
                ++self_it;
            }
            return merge_val;
        }

        template<typename F, typename M, typename Tp, typename Ops>
        [[nodiscard]] Tp slot_handler(storage::const_reference segment, F &&f, M&& m, Tp initial, Ops ops=add_op{}) const {
            Tp merge_val{initial};
            auto [word, end] = segment_slots(segment);

            for (; word != end; ++word) {
                merge_val = std::invoke(std::forward<M>(m), ops, merge_val, std::invoke(std::forward<F>(f), *word));
            }

            return merge_val;
        }

        template<typename F, typename Pr, typename M, typename Tp, typename Ops>
        [[nodiscard]] Tp slot_handler(storage::const_reference segment, F &&f, Pr &&pred, M&& m, Tp initial, Ops ops=add_op{}) const {
            Tp merge_val{initial};
            auto [word, end] = segment_slots(segment);

            for (; word != end; ++word) {
                Tp const val = std::invoke(std::forward<F>(f), *word);
                merge_val = std::invoke(std::forward<M>(m), ops, merge_val, val);
                if (!std::invoke(std::forward<Pr>(pred), val)) {
                    return merge_val;
                }
            }

            return merge_val;
        }

        template<typename Ops=add_op>
        void slot_handler(storage::reference segment, storage::const_reference other) {
            auto merge_func = dice::template_library::merge_functor<storage_word>{};
            auto [word, end] = segment_slots(segment);
            auto [word2, end2] = segment_slots(other);

            for (; word != end && word2 != end2; ++word, ++word2) {
                *word = merge_func(Ops{}, *word, *word2);
            }
        }

        bool slot_handler(storage::const_reference segment, storage::const_reference other) const {
            auto [word, end] = segment_slots(segment);
            auto [word2, end2] = segment_slots(other);

            for (; word != end && word2 != end2; ++word, ++word2) {
                if (*word != *word2) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] size_t segment_free(storage::const_reference segment) const {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::countr_zero(~segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                auto const segment_free = static_cast<const storage_word>(~word);
                return std::countr_zero(segment_free);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t segment_countl_zero(storage::const_reference segment) const noexcept {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::countl_zero(segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                return std::countl_zero(word);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t segment_countr_zero(storage::const_reference segment) const noexcept {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::countr_zero(segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                return std::countr_zero(word);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t segment_countl_one(storage::const_reference segment) const noexcept {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::countl_one(segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                return std::countl_one(word);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t segment_countr_one(storage::const_reference segment) const noexcept {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::countr_one(segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                return std::countr_one(word);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] bool segment_all_set(storage::const_reference segment) const noexcept {
            return segment_free(segment) == segment_size_in_bits;
        }

        [[nodiscard]] bool segment_any_set(storage::const_reference segment) const noexcept {
            return segment_count(segment) != 0;
        }

        [[nodiscard]] bool segment_none_set(storage::const_reference segment) const noexcept {
            return segment_free(segment) == 0x00;
        }

    public:
        using iterator = bitset_iterator;
        static constexpr Set mode_set = Set{};
        static constexpr Unset mode_unset = Unset{};

        constexpr bitset(std::initializer_list<T> const segment_v) : inner_{segment_v} {}
        constexpr bitset(Set, size_t const size) requires(!has_storage_limit) : inner_{} {
            inner_.resize(size);
            auto it = begin();
            auto it_end = end();

            while (it != it_end) {
                it++ = true;
            }
        }
        constexpr bitset(Unset, size_t const size) requires(!has_storage_limit) : inner_{} {
            inner_.resize(size);
            auto it = begin();
            auto it_end = end();

            while (it != it_end) {
                it++ = false;
            }
        }

        constexpr bitset(bitset const&) = default;
        constexpr bitset(bitset &&) = default;
        constexpr bitset &operator=(bitset const&) = default;
        constexpr bitset &operator=(bitset &&) = default;
        constexpr ~bitset() = default;

        [[nodiscard]] std::pair<storage_word_const_pointer, storage_word_const_pointer> segment_slots(storage::const_reference segment) const noexcept {
            auto word = reinterpret_cast<storage_word_const_pointer>(&segment);
            auto const end = word + segment_steps;

            return std::pair{word, end};
        }

        void set(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::segment_set, ix);
        }

        void flip(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::segment_flip, ix);
        }

        void unset(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::segment_unset, ix);
        }

        [[nodiscard]] bool test(global_ix const ix) {
            return bitset_mod_cntl(DefaultModeTag{}, &bitset::segment_test, ix);
        }

        [[nodiscard]] size_t count() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::segment_count, segment);
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t set_first_free() {
            for (auto const &segment : inner_) {
                auto offset = bitset_op_cntl(&bitset::segment_free, segment);

                if (offset != segment_size_in_bits) {
                    auto seg = std::distance(inner_.data(), &segment);
                    segment_set(seg, offset);
                    return calc_global_idx(seg, offset);
                }
            }

            if constexpr (storage::has_dynamic_extent) {
                if constexpr (!has_storage_limit) {
                    inner_.resize(inner_.size() + 1);
                    *reinterpret_cast<storage_word_pointer>(inner_.end() - 1) = 0x01;

                    return calc_global_idx(std::distance(inner_.data(), inner_.data() + size()), 0);
                }
                else {
                    if (inner_.size() == inner_.max_size()) {
                        return storage_size_in_bits;
                    }
                    inner_.resize(inner_.size() + 1);
                    *reinterpret_cast<storage_word_pointer>(inner_.end() - 1) = 0x01;

                    return calc_global_idx(std::distance(inner_.data(), inner_.data() + size()), 0);
                }
            }

            return storage_size_in_bits;
        }

        [[nodiscard]] size_t countr_zero() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::segment_countr_zero, segment);
            }, [](size_t const val) {
                return val == segment_size_in_bits;
            }, merge_functor<size_t>{}, 0, add_op{});
        }

        [[nodiscard]] size_t countl_zero() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::segment_countl_zero, segment);
            }, [](size_t const val) {
                return val == segment_size_in_bits;
            }, merge_functor<size_t>{}, 0, add_op{});
        }

        [[nodiscard]] size_t countr_one() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::segment_countr_one, segment);
            }, [](size_t const val) {
                return val == segment_size_in_bits;
            }, merge_functor<size_t>{}, 0, add_op{});
        }

        [[nodiscard]] size_t countl_one() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::segment_countl_one, segment);
            }, [](size_t const val) {
                return val == segment_size_in_bits;
            }, merge_functor<size_t>{}, 0, add_op{});
        }

        [[nodiscard]] bool all_set() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::segment_all_set, segment);
            }, [](bool const val) {
                return val;
            });
        }

        [[nodiscard]] bool any_set() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::segment_any_set, segment);
            }, merge_functor<bool>{}, false, bit_or_op{});
        }

        [[nodiscard]] bool none_set() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::segment_none_set, segment);
            }, [](bool const val) {
                return val;
            }, merge_functor<bool>{}, true, bit_and_op{});
        }

        constexpr iterator begin() noexcept {
            return iterator{*this};
        }

        constexpr std::default_sentinel_t end() const noexcept {
            return std::default_sentinel;
        }

        constexpr size_t size() const noexcept {
            return inner_.size();
        }

        constexpr size_t size_in_bits() const noexcept {
            return size() * segment_size_in_bits;
        }

        constexpr size_t inner_size() const noexcept {
            return segment_size;
        }

        constexpr size_t inner_size_in_bits() const noexcept {
            return segment_size_in_bits;
        }

        bool operator==(bitset const& alt_storage) const noexcept {
            return segment_handler([this](typename storage::const_reference segment_first, typename storage::const_reference segment_second) {
                return slot_handler(segment_first, segment_second);
            }, alt_storage);
        }

        bitset& operator<<=(size_t shift) {
            std::move(begin() + shift, begin() + size_in_bits(), begin());
            return *this;
        }

        bitset& operator>>=(size_t shift) {
            std::move_backward(begin(), begin() + size_in_bits() - shift, begin() + size_in_bits());
            return *this;
        }

        bitset& operator &=(bitset const& alt_storage) noexcept {
            segment_handler([this](typename storage::reference segment_first, typename storage::const_reference segment_second) {
                slot_handler<bit_and_op>(segment_first, segment_second);
            }, alt_storage);
            return *this;
        }

        bitset& operator |=(bitset const& alt_storage) noexcept {
            segment_handler([this](typename storage::reference segment_first, typename storage::const_reference segment_second) {
                slot_handler<bit_or_op>(segment_first, segment_second);
            }, alt_storage);
            return *this;
        }

        bitset operator<<(size_t shift) const noexcept {
            bitset tmp = *this;
            tmp <<= shift;
            return tmp;
        }

        bitset operator>>(size_t shift) const noexcept {
            bitset tmp = *this;
            tmp >>= shift;
            return tmp;
        }

        bitset operator& (bitset const& bitset_v_second) const noexcept {
            bitset tmp = *this;
            tmp &= bitset_v_second;
            return tmp;
        }

        bitset operator| (bitset const& bitset_v_second) const noexcept {
            bitset tmp = *this;
            tmp |= bitset_v_second;
            return tmp;
        }
    };

    static constexpr bitset<size_t, dynamic_extent, dynamic_extent> create_bitset(std::initializer_list<size_t> const list) {
        return bitset<size_t, dynamic_extent, dynamic_extent>{list};
    }

    template<size_t bits>
    static constexpr bitset<size_t, dynamic_extent, bits> create_bitset(std::initializer_list<size_t> const list) {
        return bitset<size_t, dynamic_extent, bits>{list};
    }

    template<typename T, size_t bits>
    static constexpr bitset<T, dynamic_extent, bits> create_bitset(std::initializer_list<T> const list) {
        return bitset<T, dynamic_extent, bits>{list};
    }

    template<typename T>
    static constexpr bitset<T, dynamic_extent, dynamic_extent> create_bitset(std::initializer_list<T> const list) {
        return bitset<T, dynamic_extent, dynamic_extent>{list};
    }
}

template<typename T, size_t extent, size_t max_extent>
struct std::formatter<dice::template_library::bitset<T, extent, max_extent>> {
    bool hex = false;
    bool debug = false;
    bool binary = false;

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        while (it != ctx.end()) {
            if (*it != 'x' && *it != '?' && *it != 'b') {
                throw std::format_error("Invalid format args for dice::template_library::bitset.");
            }

            if (*it == '?') {
                debug = true;
            }

            if (*it == 'x') {
                hex = true;
            }
            else if (*it == 'b'){
                binary = true;
            }
            ++it;
        }
    }

    auto format(dice::template_library::bitset<T, extent, max_extent> const& storage, std::format_context& ctx) const {
        auto it = storage.begin();
        auto end = storage.end();
        auto segments = storage.size();
        auto segment_size = storage.inner_size();
        auto segment_size_in_bits = storage.inner_size_in_bits();

        auto out = ctx.out();
        *out++ = '[';
        *out++ = '\n';

        if (debug) {
            out = std::format_to(out, "Segments : {}\n", segments);
            out = std::format_to(out, "Segment size : {}\n", segment_size);

            if (hex) {
                out = std::format_to(out, "Segment Representation -> Hex(0xFF)\n");
            }
            else {
                out = std::format_to(out, "Segment Representation -> Bin(0b00)\n");
            }
            *out++ = '\n';
        }

        while (it != end) {
            *out++ = '[';
            if (hex) {
                auto const& segment = it.get();
                if constexpr (std::integral<T>) {
                   out = std::format_to(out, "{:x}", segment);
                }
                else {
                    auto [word, word_end] = storage.segment_slots(segment);

                    for (; word != word_end; ++word) {
                        out = std::format_to(out, "{:x}", *word);
                    }
                }
                it.template operator++<dice::template_library::bitset_const::segment_mode>();
            }
            else if (binary) {
                for (auto segment_bit{0uz}; segment_bit < segment_size_in_bits; ++segment_bit) {
                    *out++ = *it++ ? '1' : '0';
                }
            }
            else {
                // if no mode is supported and somehow it goes throught consider just to step through, for compatible
                ++it;
            }
            *out++ = ']';
            *out++ = '\n';
        }
        *out++ = ']';
        *out++ = '\n';

        return out;
    }
};

#endif //DICE_TEMPLATELIBRARY_BITSET_HPP
