#ifndef DICE_TEMPLATELIBRARY_BITSET_HPP
#define DICE_TEMPLATELIBRARY_BITSET_HPP

#include <dice/template-library/flex_array.hpp>
#include <bit>
#include <functional>
#include <numeric>
#include <format>
#include <iterator>
#include <ranges>

namespace dice::template_library {

    ///> operation markers
    struct bit_and_op{};
    struct add_op{};
    struct bit_or_op{};

    /**
     * merge functor: merging the results of two expressions together
     */
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

    /**
     * The underlying mode to iterate over the storage
     * Used within the **bitset_iterator**
     */
    enum class bitset_mode : uint8_t {
        BitMode = 0x00,
        SegmentMode = 0x01,
    };

    /**
     * A multi-type bitset supporting both dynamic and static growth.
     * - Core operations: set(), test(), flip(), etc.
     * - Queries: any_set(), none_set(), etc.
     * - Standard bit operations
     * - Iteration at bit and segment granularity
     *
     * The segment type isn't restricted to integral types — a custom type
     * can be used as the internal segment representation.
     *
     * Examples:
     *
     * // Dynamic: grows automatically as needed
     * bitset<std::uint8_t, std::dynamic_extent, std::dynamic_extent> b{0x12, 0x13, 0x14};
     * b.set(4000uz);
     *
     * // Static: resizes within a fixed capacity, no growth
     * bitset<std::uint8_t, std::dynamic_extent, 10> b{0x12, 0x13, 0x14};
     * b.set(42uz);
     *
     * // Fixed: fixed size and storage
     * bitset<std::uint8_t, 10, 10> b{0x12, 0x13, 0x14};
     * b.set(42uz);
     *
     * @tparam T value type : any type is allowed as representation of the segment itself
     * @tparam extent extent of the bitset
     * @tparam segments max segments for the underlying bitset : use dynamic_extent to uncap limit
     */
    template<size_t extent, size_t segments, typename T = uint64_t>
    struct bitset {
    private:
        using storage   = flex_array<T, extent, segments>;
        using global_ix = size_t;
        using segment   = size_t;
        using offset    = size_t;
        
    public:
        using value_type = storage::value_type;
        using reference  = storage::reference;
        using const_reference = storage::const_reference;

    private:
        static constexpr bool   has_max_extent = storage::has_max_extent;
        static constexpr bool   has_dynamic_extent = storage::has_dynamic_extent;
        static constexpr size_t segment_size = sizeof(T);
        static constexpr size_t segment_align = alignof(T);
        static constexpr size_t segment_steps = segment_size / segment_align; ///> how many chunks fit within one segment
        static constexpr size_t segment_size_in_bits = segment_size * 8;
        static constexpr size_t storage_size = !has_max_extent ? dynamic_extent : segment_size * segments;
        static constexpr size_t storage_size_in_bits = !has_max_extent ? dynamic_extent : storage_size * 8;
        
        ///> word used for traversing the inner segment, instead of forcing 1 byte loads
        using storage_word = std::conditional_t<segment_align >= alignof(std::uint64_t), std::uint64_t,
            std::conditional_t<segment_align >= alignof(std::uint32_t), std::uint32_t,
            std::conditional_t<segment_align >= alignof(std::uint16_t), std::uint16_t, std::uint8_t>>>;
        using storage_word_pointer = storage_word *;
        using storage_word_const_pointer = storage_word const *;

        template<bool is_const>
        struct bitset_iterator {
        private:
            using bitset_pointer = std::conditional_t<is_const, bitset const*, bitset*>;

            segment       cur_segment_{};
            offset        cur_offset_{};
            bitset_pointer const backing_bitset_;

        public:
            ///> proxy for the current bit position
            struct reference {
            private:
                bitset_pointer backing_bitset_;
            public:
                segment seg;
                offset  off;

                reference(reference const&) = default;
                reference const& operator=(reference const& other) const noexcept {
                    return *this = static_cast<bool>(other);
                }

                reference(bitset_pointer backing_bitset, segment const seg, offset const off) noexcept
                    : backing_bitset_{backing_bitset}, seg{seg}, off{off} {}

                explicit operator bool() const noexcept {
                    return backing_bitset_->test(calc_global_idx(seg, off));
                }

                reference const& operator=(bool const b) const noexcept {
                    backing_bitset_->set(calc_global_idx(seg, off), b);
                    return *this;
                }
            };

            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept  = std::random_access_iterator_tag;
            using value_type        = bool;
            using pointer           = void;
            using difference_type   = ptrdiff_t;

            explicit bitset_iterator(std::conditional_t<is_const,bitset const&, bitset&> bitset) noexcept :
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

            void operator=(bool const b) const noexcept requires(!is_const) {
                backing_bitset_->set(calc_global_idx(cur_segment_, cur_offset_), b);
            }

            reference operator*() const noexcept {
                return reference {backing_bitset_, cur_segment_, cur_offset_};
            }

            // shared iterator for mode=0 (bits) mode>=1 (segments)
            template<bitset_mode mode = bitset_mode::BitMode>
            bitset_iterator& operator++() noexcept {
                if constexpr (mode == bitset_mode::BitMode) {
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

            template<bitset_mode mode = bitset_mode::BitMode>
            bitset_iterator& operator--() noexcept {
                if constexpr (mode == bitset_mode::BitMode) {
                    if (cur_offset_ == 0) {
                        --cur_segment_;
                        cur_offset_ = segment_size_in_bits-1;
                        return *this;
                    }
                    --cur_offset_;
                    return *this;
                }
                --cur_segment_;
                return *this;
            }

            template<bitset_mode mode = bitset_mode::BitMode>
            bitset_iterator operator++(int) noexcept {
                auto tmp = *this;
                operator++<mode>();
                return tmp;
            }

            template<bitset_mode mode = bitset_mode::BitMode>
            bitset_iterator operator--(int) noexcept {
                auto tmp = *this;
                operator--<mode>();
                return tmp;
            }

            template<bitset_mode mode = bitset_mode::BitMode>
            bitset_iterator& operator+=(size_t const skip) noexcept {
                auto skip_handler = [this](size_t const skip_size) {
                    auto global_ix = calc_global_idx(cur_segment_, cur_offset_) + skip_size;

                    if (global_ix >= backing_bitset_->size_in_bits()) {
                        cur_segment_ = backing_bitset_->size();
                        cur_offset_ = 0;
                        return;
                    }

                    auto offset = calc_which_offset(global_ix);
                    auto seg = calc_which_segment(global_ix);

                    cur_segment_ = seg;
                    cur_offset_ = offset;
                };

                if constexpr (mode == bitset_mode::BitMode) {
                    skip_handler(skip);
                }
                else {
                    skip_handler(skip * segment_size_in_bits);
                }

                return *this;
            }

            template<bitset_mode mode = bitset_mode::BitMode>
            bitset_iterator& operator-=(size_t const skip) noexcept {
                auto skip_handler = [this](size_t const skip_size) {
                    auto global_ix = calc_global_idx(cur_segment_, cur_offset_);

                    global_ix = global_ix >= skip_size ? global_ix - skip_size : 0;

                    auto offset = calc_which_offset(global_ix);
                    auto seg = calc_which_segment(global_ix);

                    cur_segment_ = seg;
                    cur_offset_ = offset;
                };

                if constexpr (mode == bitset_mode::BitMode) {
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

            bitset_iterator operator-(size_t rh_sub) const noexcept {
                bitset_iterator tmp = *this;
                tmp -= rh_sub;
                return tmp;
            }

            [[nodiscard]] T const& get() const noexcept {
                return *(backing_bitset_->inner_.data() + cur_segment_);
            }

            [[nodiscard]] T& get() noexcept requires(!is_const){
                return *(backing_bitset_->inner_.data() + cur_segment_);
            }

            friend bool operator==(bitset_iterator const& lhs, bitset_iterator const& rhs){
                return lhs.backing_bitset_ == rhs.backing_bitset_ &&
                       lhs.cur_segment_ == rhs.cur_segment_ &&
                       lhs.cur_offset_ == rhs.cur_offset_;
            }

            friend bool operator==(std::default_sentinel_t, bitset_iterator const& it) {
                return it == std::default_sentinel;
            }

            friend bool operator==(bitset_iterator const& it, std::default_sentinel_t) {
                return it.cur_segment_ >= it.backing_bitset_->size();
            }
        };

        template<bool is_const>
        struct position_iterator {
        private:
            using bitset_pointer = std::conditional_t<is_const, bitset const*, bitset*>;
            using reference = bitset_iterator<is_const>::reference;

            bitset_iterator<is_const> it_;
            bitset_pointer const backing_bitset_;

        public:
            explicit position_iterator(std::conditional_t<is_const, bitset const&, bitset&> bitset) noexcept
                : it_{&bitset}, backing_bitset_{&bitset} {}

            position_iterator operator++() noexcept {
                auto offset  = (*it_).off;
                auto segment = it_.get();

                if constexpr (std::integral<value_type>) {
                    if (segment == 0x00) {
                        it_ += inner_size_in_bits();
                    }
                    else {
                        it_ += offset + std::countl_zero(segment >> (offset + 1));
                    }
                }
                else {
                    auto [word, _] = backing_bitset_->segment_slots(segment);
                    for (auto i{0uz}; i < segment_steps; ++i) {
                        if (word[i] == 0x00) {
                            continue;
                        }
                        it_ += i * (sizeof(storage_word) * 8) + offset + std::countl_zero(segment >> (offset + 1));
                        break;
                    }
                }
                return *this;
            }

            position_iterator operator++(int) noexcept {
                auto tmp = *this;
                operator++();
                return tmp;
            }

            reference operator* () const noexcept {
                return *it_;
            }

            friend bool operator==(position_iterator const& lhs, position_iterator const& rhs){
                return lhs.it_ == rhs.it_;
            }

            friend bool operator==(std::default_sentinel_t, position_iterator const& it) {
                return it == std::default_sentinel;
            }

            friend bool operator==(position_iterator const& it, std::default_sentinel_t) {
                return it.it_ == std::default_sentinel;
            }
        };

        storage inner_;

        [[nodiscard]] constexpr size_t require_segments(global_ix const ix) const requires (has_dynamic_extent) {
            auto const bit_pos = bits_consumed();
            if (bit_pos > ix) {
                return 0;
            }

            auto const bit_off = ix - bit_pos;

            return [bit_off] {
                if constexpr (std::has_single_bit(segment_size_in_bits)) {
                    return (bit_off >> std::countr_zero(segment_size_in_bits)) + 1;
                }
                else {
                    return bit_off / segment_size_in_bits + 1;
                }
            }();
        }

        void constexpr expand_segments(global_ix const ix) requires (has_dynamic_extent) {
            auto const to_add = require_segments(ix);
            if (to_add == 0) {
                return;
            }

            // zero init all underlying bits when expanding x segments
            auto const old_size = inner_.size();
            inner_.resize(old_size + to_add);
            for (auto i{old_size}; i < inner_.size(); ++i) {
                inner_[i] = T{};
            }
        }

        [[nodiscard]] constexpr size_t bits_consumed() const noexcept {
            return inner_.size() * segment_size_in_bits;
        }

        [[nodiscard]] static constexpr bool fits_in_storage(global_ix const ix) noexcept {
            if constexpr (has_max_extent) {
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

        [[nodiscard]] constexpr size_t size() const noexcept {
            return inner_.size();
        }

        [[nodiscard]] static constexpr size_t inner_size() noexcept {
            return segment_size;
        }

        [[nodiscard]] static constexpr size_t inner_size_in_bits() noexcept {
            return segment_size_in_bits;
        }

        template<typename F>
        auto bitset_mod_cntl(F&& ops, global_ix const ix) -> std::invoke_result_t<F, bitset*, size_t, size_t> {
            if (!fits_in_storage(ix)) {
                throw std::out_of_range{"bitset::set: ix out of range"};
            }

            // auto expands whenever the underlying storage can actually grow
            if constexpr (has_dynamic_extent) {
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
        auto bitset_op_cntl(F &&ops, const_reference segment) const -> std::invoke_result_t<F, bitset*, const_reference> {
            if constexpr (std::is_void_v<std::invoke_result_t<F, bitset*, const_reference>>) {
                std::invoke(std::forward<F>(ops), this, segment);
                return;
            }
            else {
                return std::invoke(std::forward<F>(ops), this, segment);
            }
        }

        [[nodiscard]] static size_t offset_in_chunk(offset const o) noexcept {
            return o % (segment_align * 8);
        }

        [[nodiscard]] static size_t which_chunk(offset const o) noexcept {
            return o / (segment_align * 8);
        }

        storage_word_pointer get_chunk_raw(segment const s, offset const o) noexcept {
            return reinterpret_cast<storage_word_pointer>(inner_.data() + s) + which_chunk(o);
        }

        storage_word_const_pointer get_chunk_raw(segment const s, offset const o) const noexcept {
            return reinterpret_cast<storage_word_const_pointer>(inner_.data() + s) + which_chunk(o);
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

        [[nodiscard]] bool segment_test(segment const s, offset const o) const noexcept {
            if constexpr (std::is_integral_v<T>) {
                return *(inner_.data() + s) & 1uz << o;
            }
            else {
                auto chunk_raw = get_chunk_raw(s, o);
                return *chunk_raw & 1uz << offset_in_chunk(o);
            }
        }

        [[nodiscard]] size_t segment_count(const_reference segment) const noexcept {
            if constexpr (std::integral<value_type>) {
#ifdef __SIZEOF_INT128__
                if constexpr (std::is_same_v<value_type, __uint128_t>) {
                    uint64_t const lo = static_cast<uint64_t>(segment);
                    uint64_t const hi = static_cast<uint64_t>(segment >> 64);

                    return std::popcount(lo) + std::popcount(hi);
                }
#endif
                return std::popcount(segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                return std::popcount(word);
            }, merge_functor<storage_word>{}, 0uz, add_op{});
        }

        template<typename F, typename M, typename Tp, typename Ops>
        requires std::is_same_v<std::invoke_result_t<F, const_reference>, Tp> &&
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
        bool segment_handler(F &&handler, bitset const& other) {
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
                self_it.template operator++<bitset_mode::SegmentMode>();
                outer_it.template operator++<bitset_mode::SegmentMode>();
            }
            return true;
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
                self_it.template operator++<bitset_mode::SegmentMode>();
                outer_it.template operator++<bitset_mode::SegmentMode>();
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
                self_it.template operator++<bitset_mode::SegmentMode>();
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
                self_it.template operator++<bitset_mode::SegmentMode>();
            }
            return merge_val;
        }

        template<typename F, typename Pr, typename M, typename Tp, typename Ops>
        Tp segment_handler_backwards(F &&handler, Pr &&pred, M &&merge, Tp initial, Ops ops=add_op{}) const {
            auto self_it = begin() + size_in_bits();
            auto end_it = begin();

            Tp merge_val{initial};

            while (self_it != end_it) {
                Tp const val = std::invoke(std::forward<F>(handler), (self_it-1).get());
                merge_val = std::invoke(std::forward<M>(merge), ops, merge_val, val);
                if (!std::invoke(std::forward<Pr>(pred), val)) {
                    return merge_val;
                }
                self_it.template operator--<bitset_mode::SegmentMode>();
            }
            return merge_val;
        }

        template<typename F, typename M, typename Tp, typename Ops>
        [[nodiscard]] Tp slot_handler(const_reference segment, F &&f, M&& m, Tp initial, Ops ops=add_op{}) const {
            Tp merge_val{initial};
            auto [word, end] = segment_slots(segment);

            for (; word != end; ++word) {
                merge_val = std::invoke(std::forward<M>(m), ops, merge_val, std::invoke(std::forward<F>(f), *word));
            }

            return merge_val;
        }

        template<typename F, typename Pr, typename M, typename Tp, typename Ops>
        [[nodiscard]] Tp slot_handler(const_reference segment, F &&f, Pr &&pred, M&& m, Tp initial, Ops ops=add_op{}) const {
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

        template<typename F, typename Pr, typename M, typename Tp, typename Ops>
        [[nodiscard]] Tp slot_handler_backwards(const_reference segment, F &&f, Pr &&pred, M&& m, Tp initial, Ops ops=add_op{}) const {
            Tp merge_val{initial};
            auto [word, end] = segment_slots(segment);

            for (; end != word; --end) {
                Tp const val = std::invoke(std::forward<F>(f), *(end - 1));
                merge_val = std::invoke(std::forward<M>(m), ops, merge_val, val);
                if (!std::invoke(std::forward<Pr>(pred), val)) {
                    return merge_val;
                }
            }

            return merge_val;
        }

        template<typename Ops=add_op>
        void slot_handler(reference segment, const_reference other) {
            auto merge_func = dice::template_library::merge_functor<storage_word>{};
            auto [word, end] = segment_slots(segment);
            auto [word2, end2] = segment_slots(other);

            for (; word != end && word2 != end2; ++word, ++word2) {
                *word = merge_func(Ops{}, *word, *word2);
            }
        }

        template<typename F>
        void slot_handler(const_reference segment, F&& f) {
            auto [word, end] = segment_slots(segment);

            for (; word != end; ++word) {
                std::invoke(std::forward<F>(f), *word);
            }
        }

        void slot_handler(reference segment, storage_word val) {
            auto [word, end] = segment_slots(segment);

            for (; word != end; ++word) {
                *word = val;
            }
        }

        bool slot_handler(const_reference segment, const_reference other) const {
            auto [word, end] = segment_slots(segment);
            auto [word2, end2] = segment_slots(other);

            for (; word != end && word2 != end2; ++word, ++word2) {
                if (*word != *word2) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] size_t segment_free(const_reference segment) const {
            if constexpr (std::integral<value_type>) {
                return std::countr_zero(static_cast<storage::value_type>(~segment));
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                auto const segment_free = static_cast<const storage_word>(~word);
                return std::countr_zero(segment_free);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t segment_countl_zero(const_reference segment) const noexcept {
            if constexpr (std::integral<value_type>) {
                return std::countl_zero(segment);
            }

            return slot_handler_backwards(segment, [](storage_word word) -> size_t {
                return std::countl_zero(word);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t segment_countr_zero(const_reference segment) const noexcept {
            if constexpr (std::integral<value_type>) {
                return std::countr_zero(segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                return std::countr_zero(word);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t segment_countl_one(const_reference segment) const noexcept {
            if constexpr (std::integral<value_type>) {
                return std::countl_one(segment);
            }

            return slot_handler_backwards(segment, [](storage_word word) -> size_t {
                return std::countl_one(word);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] size_t segment_countr_one(const_reference segment) const noexcept {
            if constexpr (std::integral<value_type>) {
                return std::countr_one(segment);
            }

            return slot_handler(segment, [](storage_word word) -> size_t {
                return std::countr_one(word);
            }, [](size_t const val) {
                return val == sizeof(storage_word) * 8;
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        [[nodiscard]] bool segment_all_set(const_reference segment) const noexcept {
            return segment_count(segment) == segment_size_in_bits;
        }

        [[nodiscard]] bool segment_any_set(const_reference segment) const noexcept {
            return segment_count(segment) != 0;
        }

        [[nodiscard]] bool segment_none_set(const_reference segment) const noexcept {
            return segment_count(segment) == 0x00;
        }

    public:
        using iterator = bitset_iterator<false>;
        using const_iterator = bitset_iterator<true>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using positional_iterator = position_iterator<false>;
        using const_positional_iterator = position_iterator<true>;

        /**
         * Initializes the bitset using an initializer list
         *
         * @param segment_v initializer list of segment type
         */
        constexpr bitset(std::initializer_list<T> const segment_v) : inner_{segment_v} {}

        /**
         * Initializes the bitset using a given segment size
         * Requires the underlying storage to be uncapped
         *
         * @param size segment size to set low
         */
        explicit constexpr bitset(size_t const size) requires(!has_max_extent) : inner_{} {
            inner_.resize(size);
        }

        constexpr bitset(bitset const&) = default;
        constexpr bitset(bitset &&) = default;
        constexpr bitset &operator=(bitset const&) = default;
        constexpr bitset &operator=(bitset &&) = default;
        constexpr ~bitset() = default;

        /**
         * Get word pointers for the corresponding segment
         *
         * @return a pair containing word pointers to both segment ends (start, end)
         */
        [[nodiscard]] std::pair<storage_word_const_pointer, storage_word_const_pointer> segment_slots(const_reference segment) const noexcept {
            auto word = reinterpret_cast<storage_word_const_pointer>(&segment);
            auto const end = word + segment_steps;

            return std::pair{word, end};
        }

        /**
         * Get word pointers for the corresponding segment
         *
         * @return a pair containing word pointers to both segment ends (start, end)
         */
        [[nodiscard]] std::pair<storage_word_pointer, storage_word_pointer> segment_slots(reference segment) const noexcept {
            auto word = reinterpret_cast<storage_word_pointer>(&segment);
            auto const end = word + segment_steps;

            return std::pair{word, end};
        }

        /**
         * Set a bit high for offset ix
         *
         * @param ix offset to use
         */
        void set(global_ix const ix) {
            bitset_mod_cntl(&bitset::segment_set, ix);
        }

        /**
         * Set a bit based on $high for offset ix
         *
         * @param ix offset to use
         * @param high which bit state
         */
        void set(global_ix const ix, bool const high) {
            if (high) {
                set(ix); return;
            }
            reset(ix);
        }

        void set_all() requires(!has_dynamic_extent) {
            if constexpr (std::integral<value_type>) {
                std::fill(inner_.begin(), inner_.end(), 0x01);
            }
            else {
                for (auto &segment : inner_) {
                    slot_handler(segment, 0x01);
                }
            }
        }

        /**
         * Flip a bit for offset ix
         *
         * @param ix offset to use
         */
        void flip(global_ix const ix) {
            bitset_mod_cntl(&bitset::segment_flip, ix);
        }

        /**
         * Set a bit low for offset ix
         *
         * @param ix offset to use
         */
        void reset(global_ix const ix) {
            bitset_mod_cntl(&bitset::segment_unset, ix);
        }

        void reset_all() requires(!has_dynamic_extent) {
            if constexpr (std::integral<value_type>) {
                std::fill(inner_.begin(), inner_.end(), 0x00);
            }
            else {
                for (auto &segment : inner_) {
                    slot_handler(segment, 0x00);
                }
            }
        }

        /**
         * Test a bit (low, high) for offset ix
         *
         * @param ix offset to use
         *
         * @return bool indicating the state of ix
         */
        [[nodiscard]] bool test(global_ix const ix) const {
            if (!fits_in_storage(ix)) {
                throw std::out_of_range{"bitset::set: ix out of range"};
            }

            auto const segment = calc_which_segment(ix);
            auto const offset  = calc_which_offset(ix);
            return segment_test(segment, offset);
        }

        /**
         * Compacts the underlying storage backend, if applicable
         */
        void shrink_to_fit() requires (has_dynamic_extent){
            auto it = begin() + size_in_bits();
            auto end = begin();

            auto perform_shrink = [this](auto const& segment) {
                auto ptr_dist = std::distance(inner_.data(), &segment);
                if constexpr (!has_max_extent) {
                    inner_ = storage(inner_.data(), inner_.data() + ptr_dist);
                }
                else {
                    inner_.resize(std::distance(inner_.data(), &segment));
                }
            };

            while (it != end) {
                auto segment = (it - 1).get();
                if constexpr (std::integral<value_type>) {
                    if (static_cast<value_type>(~segment) != 0x00) {
                        perform_shrink(segment);
                        return;
                    }
                }
                else {
                    auto has_bit = slot_handler(segment, [](storage_word word) {
                        return static_cast<storage_word>(~word) != 0x00;
                    }, merge_functor<bool>{}, false, bit_or_op{});

                    if (has_bit) {
                        perform_shrink(segment);
                        return;
                    }
                }
                it.template operator--<bitset_mode::SegmentMode>();
            }
        }

        /**
         * Counts the total bits set
         *
         * @return total bits set
         */
        [[nodiscard]] size_t count() const {
            return segment_handler([this](const_reference segment) {
                return bitset_op_cntl(&bitset::segment_count, segment);
            }, merge_functor<size_t>{}, 0uz, add_op{});
        }

        /**
         * Sets the first bit high, and returns the corresponding index
         *
         * @return ix to first free index in bitset (if there is no free index, storage_size_in_bits is returned)
         */
        [[nodiscard]] size_t set_first_free() {
            for (auto &segment : inner_) {
                auto offset = bitset_op_cntl(&bitset::segment_free, segment);

                if (offset != segment_size_in_bits) {
                    auto seg = std::distance(inner_.data(), &segment);
                    segment_set(seg, offset);
                    return calc_global_idx(seg, offset);
                }
            }

            if constexpr (has_dynamic_extent) {
                if constexpr (!has_max_extent) {
                    inner_.resize(inner_.size() + 1);
                    *reinterpret_cast<storage_word_pointer>(inner_.end() - 1) = 0x01;

                    return calc_global_idx(size() - 1, 0);
                }
                else {
                    if (inner_.size() == inner_.max_size()) {
                        return storage_size_in_bits;
                    }
                    inner_.resize(inner_.size() + 1);
                    *reinterpret_cast<storage_word_pointer>(inner_.end() - 1) = 0x01;

                    return calc_global_idx(size() - 1, 0);
                }
            }

            return storage_size_in_bits;
        }

        /**
         * Counts consecutive zeros starting from LSB
         *
         * @return ix to non-zero entry
         */
        [[nodiscard]] size_t countr_zero() const {
            return segment_handler([this](const_reference segment) {
                return bitset_op_cntl(&bitset::segment_countr_zero, segment);
            }, [](size_t const val) {
                return val == segment_size_in_bits;
            }, merge_functor<size_t>{}, 0, add_op{});
        }

        /**
         * Counts consecutive zeros starting from MSB
         *
         * @return ix to non-zero entry
         */
        [[nodiscard]] size_t countl_zero() const {
            return segment_handler_backwards([this](const_reference segment) {
                return bitset_op_cntl(&bitset::segment_countl_zero, segment);
            }, [](size_t const val) {
                return val == segment_size_in_bits;
            }, merge_functor<size_t>{}, 0, add_op{});
        }

        /**
         * Counts consecutive zeros starting from LSB
         *
         * @return ix to zero entry
         */
        [[nodiscard]] size_t countr_one() const {
            return segment_handler([this](const_reference segment) {
                return bitset_op_cntl(&bitset::segment_countr_one, segment);
            }, [](size_t const val) {
                return val == segment_size_in_bits;
            }, merge_functor<size_t>{}, 0, add_op{});
        }

        /**
         * Counts consecutive ones starting from MSB
         *
         * @return ix to zero entry
         */
        [[nodiscard]] size_t countl_one() const {
            return segment_handler_backwards([this](const_reference segment) {
                return bitset_op_cntl(&bitset::segment_countl_one, segment);
            }, [](size_t const val) {
                return val == segment_size_in_bits;
            }, merge_functor<size_t>{}, 0, add_op{});
        }

        /**
         * Returns a bool indicating if all bits are set or not
         *
         * @return queried state
         */
        [[nodiscard]] bool all_set() const {
            return segment_handler([this](const_reference segment) {
                return bitset_op_cntl(&bitset::segment_all_set, segment);
            }, [](bool const val) {
                return val;
            });
        }

        /**
         * Returns a bool indicating if any bit is set
         *
         * @return queried state
         */
        [[nodiscard]] bool any_set() const {
            return segment_handler([this](const_reference segment) {
                return bitset_op_cntl(&bitset::segment_any_set, segment);
            }, merge_functor<bool>{}, false, bit_or_op{});
        }

        /**
         * Returns a bool indicating if no bit is set
         *
         * @return queried state
         */
        [[nodiscard]] bool none_set() const {
            return segment_handler([this](const_reference segment) {
                return bitset_op_cntl(&bitset::segment_none_set, segment);
            }, [](bool const val) {
                return val;
            }, merge_functor<bool>{}, true, bit_and_op{});
        }

        auto positions() const -> std::ranges::input_range auto {
            return *this | std::views::filter([] (auto bitref) {
                return static_cast<bool>(bitref);
            }) | std::views::transform([] (auto bitref) {
                return calc_global_idx(bitref.seg, bitref.off);
            });
        }

        template<typename F>
        void positions_cntl(std::ranges::input_range auto&& positions, F&& pos_f) {
            if constexpr (std::ranges::sized_range<decltype(positions)>) {
                auto position_elements = std::ranges::size(positions);
                if (position_elements >= storage_size_in_bits) {
                    throw std::range_error{"bitset::set_positions range out of bounds"};
                }
            }
            else {
                auto position_elements = std::ranges::distance(positions);
                if (position_elements >= storage_size_in_bits) {
                    throw std::range_error{"bitset::set_positions range out of bounds"};
                }
            }

            std::ranges::for_each(positions, std::forward<F>(pos_f));
        }

        void set_positions(std::ranges::input_range auto&& positions) {
            positions_cntl(std::forward<decltype(positions)>(positions), [this](auto pos) {
                set(pos);
            });
        }

        void reset_positions(std::ranges::input_range auto&& positions) {
            positions_cntl(std::forward<decltype(positions)>(positions), [this](auto pos) {
                reset(pos);
            });
        }

        constexpr iterator begin() noexcept {
            return iterator{*this};
        }

        constexpr const_iterator begin() const noexcept {
            return const_iterator{*this};
        }

        constexpr reverse_iterator rbegin() noexcept {
            return reverse_iterator{begin() + size_in_bits()};
        }

        constexpr const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator{begin() + size_in_bits()};
        }

        constexpr reverse_iterator rend() noexcept {
            return reverse_iterator{begin()};
        }

        constexpr const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator{begin()};
        }

        constexpr std::default_sentinel_t end() const noexcept {
            return std::default_sentinel;
        }

        constexpr positional_iterator pbegin() noexcept {
            return positional_iterator{*this};
        }

        constexpr positional_iterator pbegin() const noexcept {
            return positional_iterator{*this};
        }

        constexpr std::default_sentinel_t pend() const noexcept {
            return std::default_sentinel;
        }

        /**
         * Returns consumed bits in bits
         *
         * @return consumed bits
         */
        constexpr size_t size_in_bits() const noexcept {
            return size() * segment_size_in_bits;
        }

        bool operator==(bitset const& alt_storage) const noexcept {
            return segment_handler([this](const_reference segment_first, const_reference segment_second) {
                return slot_handler(segment_first, segment_second);
            }, alt_storage);
        }

        bitset& operator<<=(size_t shift) {
            auto dest_it = std::move(begin() + shift, begin() + size_in_bits(), begin());
            std::fill(dest_it, begin() + size_in_bits(), false);
            return *this;
        }

        bitset& operator>>=(size_t shift) {
            auto dest_it = std::move_backward(begin(), begin() + size_in_bits() - shift, begin() + size_in_bits());
            std::fill(begin(), dest_it, false);
            return *this;
        }

        bitset& operator&=(bitset const& alt_storage) noexcept {
            segment_handler([this](reference segment_first, const_reference segment_second) {
                slot_handler<bit_and_op>(segment_first, segment_second);
                return true;
            }, alt_storage);
            return *this;
        }

        bitset& operator|=(bitset const& alt_storage) noexcept {
            segment_handler([this](reference segment_first, const_reference segment_second) {
                slot_handler<bit_or_op>(segment_first, segment_second);
                return true;
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

        bitset operator&(bitset const& bitset_v_second) const noexcept {
            bitset tmp = *this;
            tmp &= bitset_v_second;
            return tmp;
        }

        bitset operator|(bitset const& bitset_v_second) const noexcept {
            bitset tmp = *this;
            tmp |= bitset_v_second;
            return tmp;
        }
    };
}

template<typename T, size_t extent, size_t max_extent>
struct std::formatter<dice::template_library::bitset<extent, max_extent, T>> {
    bool hex = false;
    bool debug = false;
    bool binary = false;

    ///> parse formatter context, only allowing hex, debug and binary symbol
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        while (it != ctx.end() && *it != '}') {
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
        return it;
    }

    auto format(dice::template_library::bitset<extent, max_extent, T> const& storage, std::format_context& ctx) const {
        auto it = storage.begin();
        auto end = storage.end();
        auto segments = storage.size();
        auto segment_size_in_bits = storage.inner_size_in_bits();
        auto storage_size_in_bits = storage.storage_size_in_bits;

        auto out = ctx.out();
        *out++ = '[';
        *out++ = '\n';

        if (debug) {
            out = std::format_to(out, "Segments : {}\n", segments);
            out = std::format_to(out, "Segment size : {}\n", segment_size_in_bits);
            out = std::format_to(out, "Storage size : {}\n", storage_size_in_bits);

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
                   out = std::format_to(out, "{:#0{}x}", segment, (storage.segment_align * 8 / 4));
                }
                else {
                    auto [word, word_end] = storage.segment_slots(segment);

                    for (; word != word_end; ++word) {
                        out = std::format_to(out, "{:#0{}x}", *word, (storage.segment_align * 8) / 4);
                    }
                }
                it.template operator++<dice::template_library::bitset_mode::SegmentMode>();
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
