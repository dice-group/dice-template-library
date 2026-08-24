#ifndef DICE_TEMPLATELIBRARY_BITSET_HPP
#define DICE_TEMPLATELIBRARY_BITSET_HPP

#include <bit>
#include <compare>
#include <dice/template-library/flex_array.hpp>
#include <dice/template-library/memfn.hpp>
#include <format>
#include <functional>
#include <iterator>
#include <ranges>
#include <algorithm>

namespace dice::template_library {
    /**
     * A bitset supporting both dynamic and static growth.
     * - Core operations: set(), test(), flip(), etc.
     * - Queries: any_set(), none_set(), etc.
     * - Standard bit operations
     * - Iteration at bit and segment granularity
     * - Positional Iteration
     *
     * Examples:
     *
     * // Dynamic: grows automatically as needed
     * bitset<std::dynamic_extent, std::dynamic_extent, std::uint8_t> b{0x12, 0x13, 0x14};
     * b.set(4000uz);
     *
     * // Static: resizes within a fixed capacity, no growth
     * bitset<std::dynamic_extent, 10, std::uint8_t> b{0x12, 0x13, 0x14};
     * b.set(42uz);
     *
     * // Fixed: fixed size and storage
     * bitset<10, 10, std::uint8_t> b{0x12, 0x13, 0x14};
     * b.set(42uz);
     *
     * // Default type: (Dynamic)
     * bitset<std::dynamic_extent, std::dynamic_extent> ...
     *
     * // Default type: (Static)
     * bitset<10, 10> ...
     *
     * @tparam T value type : any type is allowed as representation of the underlying storage
     * @tparam bits extent of the bitset
     * @tparam max_bits minimal bits for the underlying bitset : use dynamic_extent to uncap limit
     */
    template<size_t bits, size_t max_bits, typename T = uint64_t>
    requires std::unsigned_integral<T>
    struct bitset {
    private:
        static constexpr size_t segment_size = sizeof(T);
        static constexpr size_t segment_size_in_bits = segment_size * 8;

        static constexpr size_t segments = bits; // just forward this value
        static constexpr size_t max_segments = max_bits != dynamic_extent ? (max_bits + segment_size_in_bits - 1) / segment_size_in_bits : dynamic_extent;

        using storage = flex_array<T, segments, max_segments>;
        using global_ix = size_t;
        using segment = size_t;
        using offset = size_t;

        using segment_type = storage::value_type;
        using segment_reference = storage::reference;
        using segment_const_reference = storage::const_reference;
    public:
        ///> mode to be used for the underlying iterator - make caller enforce policy
        enum struct bitset_mode : uint8_t {
            BitMode = 0x00,
            SegmentMode = 0x01,
        };

    private:
        static constexpr bool has_max_extent = storage::has_max_extent;
        static constexpr bool has_dynamic_extent = storage::has_dynamic_extent;

        static constexpr size_t storage_size = !has_max_extent ? dynamic_extent : segment_size * max_segments;
        static constexpr size_t storage_size_in_bits = !has_max_extent ? dynamic_extent : storage_size * 8;

        template<bool is_const, bitset_mode mode = bitset_mode::BitMode>
        struct bitset_iterator {
        private:
            using bitset_pointer = std::conditional_t<is_const, bitset const *, bitset *>;
            static constexpr bool using_bit_mode = mode == bitset_mode::BitMode;

            segment cur_segment_{};
            offset cur_offset_{};
            bitset_pointer backing_bitset_ = nullptr;

        public:
            ///> proxy for the current bit position
            struct reference {
            private:
                bitset_pointer backing_bitset_;

            public:
                segment seg;
                offset off;

                reference(reference const &) = default;
                reference const &operator=(reference const &other) const noexcept {
                    return *this = static_cast<bool>(other);
                }

                reference(bitset_pointer backing_bitset, segment const seg, offset const off) noexcept
                    : backing_bitset_{backing_bitset},
                      seg{seg},
                      off{off} {
                }

                operator bool() const noexcept requires (using_bit_mode)
                {
                    return backing_bitset_->test(calc_global_idx(seg, off));
                }

                operator std::conditional_t<is_const, T const &, T &>() const noexcept requires (!using_bit_mode)
                {
                    return *(backing_bitset_->inner_.data() + seg);
                }

                reference const &operator=(bool const b) const noexcept {
                    backing_bitset_->set(calc_global_idx(seg, off), b);
                    return *this;
                }

                [[nodiscard]] size_t ix() const noexcept {
                    return calc_global_idx(seg, off);
                }
            };

            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using value_type = std::conditional_t<using_bit_mode, bool, T>;
            using pointer = void;
            using difference_type = ptrdiff_t;

            bitset_iterator() noexcept = default;

            explicit bitset_iterator(std::conditional_t<is_const, bitset const &, bitset &> bitset) noexcept
                : backing_bitset_{&bitset} {
            }

            explicit bitset_iterator(bitset &bitset, offset const &o)
                : backing_bitset_{&bitset} {
                if (o >= segment_size_in_bits) {
                    throw std::out_of_range{"bitset_iterator: o >= segment_size"};
                }

                if (calc_global_idx(cur_segment_, o) >= bitset.logical_size()) {
                    throw std::out_of_range{"bitset_iterator: index out of bounds"};
                }

                cur_offset_ = o;
            }

            explicit bitset_iterator(bitset &bitset, offset const &o, segment const &s)
                : backing_bitset_{&bitset} {
                if (o >= segment_size_in_bits) {
                    throw std::out_of_range{"bitset_iterator: o >= segment_size"};
                }

                if (s >= bitset.size()) {
                    throw std::out_of_range{"bitset_iterator: segment out of bounds"};
                }

                if (calc_global_idx(s, o) >= bitset.logical_size()) {
                    throw std::out_of_range{"bitset_iterator: index out of bounds"};
                }

                cur_offset_ = o;
                cur_segment_ = s;
            }

            reference operator*() const noexcept {
                return reference{backing_bitset_, cur_segment_, cur_offset_};
            }

            reference operator[](size_t ix) const noexcept {
                return *(*this + ix);
            }

            bitset_iterator &operator++() noexcept {
                if constexpr (mode == bitset_mode::BitMode) {
                    ++cur_offset_;
                    if (calc_global_idx(cur_segment_, cur_offset_) >= backing_bitset_->logical_size()) {
                        cur_segment_ = backing_bitset_->size();
                        cur_offset_ = 0;
                        return *this;
                    }
                    if (cur_offset_ >= segment_size_in_bits) {
                        ++cur_segment_;
                        cur_offset_ = 0;
                        return *this;
                    }
                    return *this;
                }
                ++cur_segment_;
                return *this;
            }

            bitset_iterator &operator--() noexcept {
                if constexpr (mode == bitset_mode::BitMode) {
                    if (cur_offset_ == 0) {
                        --cur_segment_;
                        cur_offset_ = segment_size_in_bits - 1;
                        return *this;
                    }
                    --cur_offset_;
                    return *this;
                }
                --cur_segment_;
                return *this;
            }

            bitset_iterator operator++(int) noexcept {
                auto tmp = *this;
                operator++();
                return tmp;
            }

            bitset_iterator operator--(int) noexcept {
                auto tmp = *this;
                operator--();
                return tmp;
            }

            bitset_iterator &operator+=(difference_type const skip) noexcept {
                if (skip < 0) {
                    return operator-=(-skip);
                }

                assert(skip >= 0);

                auto skip_handler = [this](size_t const skip_size) {
                    auto global_ix = calc_global_idx(cur_segment_, cur_offset_) + skip_size;

                    if (global_ix >= backing_bitset_->logical_size()) {
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
                } else {
                    skip_handler(skip * segment_size_in_bits);
                }

                return *this;
            }

            bitset_iterator &operator-=(difference_type const skip) noexcept {
                if (skip < 0) {
                    return operator+=(-skip);
                }

                assert(skip >= 0);

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
                } else {
                    skip_handler(skip * segment_size_in_bits);
                }

                return *this;
            }

            bitset_iterator operator+(difference_type rh_add) const noexcept {
                bitset_iterator tmp = *this;
                tmp += rh_add;
                return tmp;
            }

            bitset_iterator operator-(difference_type rh_sub) const noexcept {
                bitset_iterator tmp = *this;
                tmp -= rh_sub;
                return tmp;
            }

            difference_type operator-(bitset_iterator const &other) const noexcept {
                return (**this).ix() - (*other).ix();
            }

            [[nodiscard]] T const &get() const noexcept {
                return *(backing_bitset_->inner_.data() + cur_segment_);
            }

            [[nodiscard]] T &get() noexcept requires (!is_const)
            {
                return *(backing_bitset_->inner_.data() + cur_segment_);
            }

            friend bitset_iterator operator-(difference_type lh_sub, bitset_iterator const &rhs) noexcept {
                return rhs - lh_sub;
            }

            friend bitset_iterator operator+(difference_type lh_add, bitset_iterator const &rhs) noexcept {
                return rhs + lh_add;
            }

            friend std::strong_ordering operator<=>(bitset_iterator const &lhs, bitset_iterator const &rhs) noexcept {
                // UB comparing different bitsets
                assert(lhs.backing_bitset_ == rhs.backing_bitset_);

                if (lhs == rhs) {
                    return std::strong_ordering::equal;
                }

                auto const lhs_ix = calc_global_idx(lhs.cur_segment_, lhs.cur_offset_);
                auto const rhs_ix = calc_global_idx(rhs.cur_segment_, rhs.cur_offset_);

                if (lhs_ix > rhs_ix) {
                    return std::strong_ordering::greater;
                }

                return std::strong_ordering::less;
            }

            friend bool operator==(bitset_iterator const &lhs, bitset_iterator const &rhs) {
                return lhs.backing_bitset_ == rhs.backing_bitset_ && lhs.cur_segment_ == rhs.cur_segment_ && lhs.cur_offset_ == rhs.cur_offset_;
            }

            friend bool operator==(std::default_sentinel_t, bitset_iterator const &it) {
                return it == std::default_sentinel;
            }

            friend bool operator==(bitset_iterator const &it, std::default_sentinel_t) {
                return calc_global_idx(it.cur_segment_, it.cur_offset_) >= it.backing_bitset_->logical_size();
            }
        };

        ///> enforce only bit mode in the position iterator
        template<bool is_const>
        struct position_iterator {
        private:
            using bitset_pointer = std::conditional_t<is_const, bitset const *, bitset *>;
            using reference = bitset_iterator<is_const>::reference;

            bitset_iterator<is_const> it_;
            bitset_pointer backing_bitset_ = nullptr;

            ///> add bool to allow setting a start point, without skipping the initial offset
            void seek(bool include_current) noexcept {
                auto offset = (*it_).off;
                auto segment = it_.get();

                auto skip = include_current ? offset : offset + 1;

                while (true) {
                    T const shifted = (skip >= segment_size_in_bits)
                                          ? T{}
                                          : static_cast<T>(segment >> skip);

                    if (shifted != 0x00) {
                        it_ += (skip + std::countr_zero(shifted)) - offset;
                        break;
                    }

                    it_ += segment_size_in_bits - offset;
                    if (it_ == std::default_sentinel) {
                        break;
                    }

                    offset = 0;
                    skip = 0;
                    segment = it_.get();
                }
            }

        public:
            using iterator_category = std::input_iterator_tag;
            using iterator_concept = std::input_iterator_tag;
            using value_type = size_t;
            using pointer = void;
            using difference_type = ptrdiff_t;

            explicit position_iterator(std::conditional_t<is_const, bitset const &, bitset &> bitset) noexcept
                : it_{bitset},
                  backing_bitset_{&bitset} {
                if (it_ != std::default_sentinel) {
                    seek(true);
                }
            }

            position_iterator &operator++() noexcept {
                seek(false);
                return *this;
            }

            position_iterator operator++(int) noexcept {
                auto tmp = *this;
                operator++();
                return tmp;
            }

            value_type operator*() const noexcept {
                return (*it_).ix();
            }

            friend bool operator==(position_iterator const &lhs, position_iterator const &rhs) {
                return lhs.it_ == rhs.it_;
            }

            friend bool operator==(std::default_sentinel_t, position_iterator const &it) {
                return it == std::default_sentinel;
            }

            friend bool operator==(position_iterator const &it, std::default_sentinel_t) {
                return it.it_ == std::default_sentinel;
            }
        };

        using sub_range_type = std::ranges::subrange<bitset_iterator<false, bitset_mode::SegmentMode>>;
        using const_sub_range_type = std::ranges::subrange<bitset_iterator<true, bitset_mode::SegmentMode>>;

        storage inner_;
        size_t bits_{};

        [[nodiscard]] constexpr size_t require_segments(global_ix const ix) requires (has_dynamic_extent)
        {
            auto const bit_pos = logical_size();
            if (bit_pos > ix) {
                return 0;
            }
            bits_ = ix + 1;

            if (ix < capacity_in_bits()) {
                return 0; // fits within current segment
            }

            return calc_which_segment(ix) - size() + 1;
        }

        constexpr void expand_segments(global_ix const ix) requires (has_dynamic_extent)
        {
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

        [[nodiscard]] static constexpr bool fits_in_storage(global_ix const ix) noexcept {
            if constexpr (has_max_extent) {
                return ix < max_bits;
            }

            return true;
        }

        [[nodiscard]] constexpr bool is_aligned() const noexcept {
            return leftover_bits() == 0;
        }

        [[nodiscard]] constexpr size_t leftover_bits() const noexcept {
            return logical_size() % segment_size_in_bits;
        }

        [[nodiscard]] static constexpr segment calc_which_segment(global_ix const ix) noexcept {
            return ix >> std::countr_zero(segment_size_in_bits);
        }

        [[nodiscard]] static constexpr offset calc_which_offset(global_ix const ix) noexcept {
            return ix & (segment_size_in_bits - 1);
        }

        [[nodiscard]] static constexpr global_ix calc_global_idx(segment const s, offset const o) noexcept {
            return s * segment_size_in_bits + o;
        }

        [[nodiscard]] constexpr size_t logical_size() const noexcept {
            if constexpr (!has_dynamic_extent) {
                return max_bits;
            } else {
                return bits_;
            }
        }

        [[nodiscard]] constexpr size_t size() const noexcept {
            return inner_.size();
        }

        template<typename F>
        auto bitset_mod_cntl(F &&ops, global_ix const ix) -> std::invoke_result_t<F, size_t, size_t> {
            if (!fits_in_storage(ix)) {
                throw std::out_of_range{"bitset::set: ix out of range"};
            }

            // auto expands whenever the underlying storage can actually grow
            if constexpr (has_dynamic_extent) {
                expand_segments(ix);
            }

            auto const segment = calc_which_segment(ix);
            auto const offset = calc_which_offset(ix);

            return std::invoke(std::forward<F>(ops), segment, offset);
        }

        void segment_set(segment const s, offset const o) noexcept {
            *(inner_.data() + s) |= T{1} << o;
        }

        void segment_flip(segment const s, offset const o) noexcept {
            *(inner_.data() + s) ^= T{1} << o;
        }

        void segment_unset(segment const s, offset const o) noexcept {
            *(inner_.data() + s) &= static_cast<T>(~(T{1} << o));
        }

        [[nodiscard]] bool segment_test(segment const s, offset const o) const noexcept {
            return *(inner_.data() + s) & T{1} << o;
        }

        ///> folds across every segment via merge, no early exit
        template<typename F, typename M, typename Tp, typename Range>
        requires std::is_same_v<std::invoke_result_t<F, segment_const_reference>, Tp> && std::invocable<M, Tp, Tp>
        [[nodiscard]] Tp segments_reduce(F &&handler, M &&merge, Tp initial, Range const &sub_range) const {
            Tp merge_val{initial};
            for (auto const &segment : sub_range) {
                merge_val = std::invoke(merge, merge_val, std::invoke(handler, segment));
            }

            return merge_val;
        }

        ///> in-place unary transform
        template<typename Ops>
        void segments_transform(sub_range_type const &sub_range) {
            auto ops = Ops{};

            for (auto &&proxy : sub_range) {
                T &segment = proxy;
                segment = ops(segment);
            }
        }

        ///> in-place binary transform
        template<typename Ops>
        void segments_transform_with(bitset const &other, size_t stop=0) {
            auto self_it = segments_begin();
            auto outer_it = other.segments_begin();
            auto ops = Ops{};

            if (logical_size() != other.logical_size()) {
                return;
            }

            auto end_sentinel = segments_begin() + size() - stop;

            while (self_it != end_sentinel) {
                auto &seg_this = self_it.get();
                auto &seg_other = outer_it.get();
                seg_this = ops(seg_this, seg_other);

                ++self_it;
                ++outer_it;
            }
        }

        ///> true iff sizes match and handler(seg_this, seg_other) holds for every segment pair
        template<typename F>
        bool segments_pairwise_all_of(F &&handler, bitset const &other) const {
            auto self_it = segments_begin();
            auto outer_it = other.segments_begin();

            if (logical_size() != other.logical_size()) {
                return false;
            }

            auto end_sentinel = end();

            while (self_it != end_sentinel) {
                if (!std::invoke(handler, self_it.get(), outer_it.get())) {
                    return false;
                }
                ++self_it;
                ++outer_it;
            }
            return true;
        }

        ///> true iff pred(handler(segment)) holds for every segment, early-exit on the first failure
        template<typename F, typename Pr, typename Range>
        static bool segments_all_of(F &&handler, Pr &&pred, Range const &sub_range) {
            for (auto const &segment : sub_range) {
                if (auto const val = std::invoke(handler, segment); !std::invoke(pred, val)) {
                    return false;
                }
            }
            return true;
        }

        ///> folds handler(segment) across every segment, stopping early once pred(val) fails
        template<typename F, typename Pr, typename M, typename Tp, typename Range>
        static Tp segments_reduce_while(F &&handler, Pr &&pred, M &&merge, Tp initial, Range const &sub_range) {
            Tp merge_val{initial};

            for (auto const &segment : sub_range) {
                Tp const val = std::invoke(handler, segment);
                merge_val = std::invoke(merge, merge_val, val);
                if (!std::invoke(pred, val)) {
                    return merge_val;
                }
            }
            return merge_val;
        }

        ///> same as segments_reduce_while, but iterates segments in reverse order
        template<typename F, typename Pr, typename M, typename Tp, typename Range>
        static Tp segments_reduce_while_reverse(F &&handler, Pr &&pred, M &&merge, Tp initial, Range const &sub_range) {
            auto reversed_sub_range = sub_range | std::views::reverse;

            Tp merge_val{initial};

            for (auto const &segment : reversed_sub_range) {
                Tp const val = std::invoke(handler, segment);
                merge_val = std::invoke(merge, merge_val, val);
                if (!std::invoke(pred, val)) {
                    return merge_val;
                }
            }
            return merge_val;
        }

        [[nodiscard]] static bool segment_all_set(segment_const_reference segment) noexcept {
            return std::popcount(segment) == segment_size_in_bits;
        }

        [[nodiscard]] static bool segment_any_set(segment_const_reference segment) noexcept {
            return std::popcount(segment) != 0x00;
        }

        [[nodiscard]] static bool segment_none_set(segment_const_reference segment) noexcept {
            return std::popcount(segment) == 0x00;
        }

        [[nodiscard]] bool size_match(bitset const& other) const noexcept {
            return logical_size() == other.logical_size();
        }

        [[nodiscard]] std::pair<const_sub_range_type, std::optional<segment_type>> full_segments_or() const noexcept {
            if (is_aligned()) {
                return std::make_pair(std::ranges::subrange(segments_begin(), segments_begin() + size()), std::nullopt);
            }
            auto full_segments = size() - 1;
            return std::make_pair(std::ranges::subrange(segments_begin(), segments_begin() + full_segments), std::make_optional<segment_type>((segments_begin() + full_segments).get()));
        }

        [[nodiscard]] std::pair<sub_range_type, std::optional<segment_type>> full_segments_or() noexcept {
            if (is_aligned()) {
                return std::make_pair(std::ranges::subrange(segments_begin(), segments_begin()), std::nullopt);
            }
            auto full_segments = size() - 1;
            return std::make_pair(std::ranges::subrange(segments_begin(), segments_begin() + full_segments), std::make_optional<segment_type>((segments_begin() + full_segments).get()));
        }

        [[nodiscard]] const_sub_range_type full_segment() const noexcept {
            return std::ranges::subrange(segments_begin(), segments_begin() + size());
        }

        [[nodiscard]] sub_range_type full_segment() noexcept {
            return std::ranges::subrange(segments_begin(), segments_begin() + size());
        }

        template<bitset_mode mode>
        using iterator = bitset_iterator<false, mode>;

        template<bitset_mode mode>
        using const_iterator = bitset_iterator<true, mode>;

        template<bitset_mode mode>
        using reverse_iterator = std::reverse_iterator<iterator<mode>>;

        template<bitset_mode mode>
        using const_reverse_iterator = std::reverse_iterator<const_iterator<mode>>;

    public:
        using positional_iterator = position_iterator<false>;
        using const_positional_iterator = position_iterator<true>;

        using bit_iterator = iterator<bitset_mode::BitMode>;
        using const_bit_iterator = const_iterator<bitset_mode::BitMode>;
        using reverse_iterator_t = reverse_iterator<bitset_mode::BitMode>;
        using const_reverse_iterator_t = const_reverse_iterator<bitset_mode::BitMode>;

        using reference = bit_iterator::reference;
        using value_type = bit_iterator::value_type;

        /**
         * Empty bitset
         */
        explicit constexpr bitset() = default;

        /**
         * Initializes the bitset using an initializer list
         *
         * @param segment_v initializer list of segment type
         */
        constexpr bitset(std::initializer_list<T> const segment_v)
            : inner_{segment_v},
              bits_{segment_size_in_bits * segment_v.size()} {
        }

        /**
         * Initializes the bitset using a given segment size
         * Requires the underlying storage to be uncapped
         *
         * @param size segment size to set low
         */
        explicit constexpr bitset(size_t const size) requires (!has_max_extent)
            : inner_{},
              bits_{segment_size_in_bits * size} {
            inner_.resize(size);
        }

        constexpr bitset(bitset const &) = default;
        constexpr bitset(bitset &&) = default;
        constexpr bitset &operator=(bitset const &) = default;
        constexpr bitset &operator=(bitset &&) = default;
        constexpr ~bitset() = default;

        /**
         * Set a bit high for offset ix
         *
         * @param ix offset to use
         */
        void set(global_ix const ix) {
            bitset_mod_cntl(DICE_MEMFN(segment_set), ix);
        }

        /**
         * Set a bit based on $high for offset ix
         *
         * @param ix offset to use
         * @param high which bit state
         */
        void set(global_ix const ix, bool const high) {
            if (high) {
                set(ix);
                return;
            }
            reset(ix);
        }

        void set_all() {
            if (is_aligned()) {
                std::fill(inner_.begin(), inner_.end(), static_cast<T>(~T{}));
                return;
            }
            std::fill(inner_.begin(), inner_.end() - 1, static_cast<T>(~T{}));
            *(inner_.end() - 1) = static_cast<T>(static_cast<T>(~T{}) >> (segment_size_in_bits - leftover_bits()));
        }

        /**
         * Flip a bit for offset ix
         *
         * @param ix offset to use
         */
        void flip(global_ix const ix) {
            bitset_mod_cntl(DICE_MEMFN(segment_flip), ix);
        }

        /**
         * Set a bit low for offset ix
         *
         * @param ix offset to use
         */
        void reset(global_ix const ix) {
            bitset_mod_cntl(DICE_MEMFN(segment_unset), ix);
        }

        void reset_all() {
            std::fill(inner_.begin(), inner_.end(), T{});
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

            // if the ix is not in the bits consumed range, return false
            if (ix >= logical_size()) {
                return false;
            }

            auto const segment = calc_which_segment(ix);
            auto const offset = calc_which_offset(ix);
            return segment_test(segment, offset);
        }

        /**
         * Compacts the underlying storage backend, if applicable
         */
        void shrink_to_fit() requires (has_dynamic_extent)
        {
            auto it = segments_rbegin();
            auto end = segments_rend();

            while (it != end) {
                T &segment = *it;
                if (segment != 0x00) {
                    auto ptr_dist = std::distance(inner_.data(), &segment) + 1; // this segment should also be included !
                    if constexpr (!has_max_extent) {
                        inner_ = storage(inner_.data(), inner_.data() + ptr_dist);
                    } else {
                        inner_.resize(ptr_dist);
                    }
                    bits_ = ptr_dist * segment_size_in_bits;
                    return;
                }
                ++it;
            }
            // zero segments - shrink to zero
            if constexpr (!has_max_extent) {
                inner_ = storage(inner_.data(), inner_.data());
            } else {
                inner_.resize(0);
            }
            bits_ = 0;
        }

        /**
         * Counts the total bits set
         *
         * @return total bits set
         */
        [[nodiscard]] size_t count() const {
            return segments_reduce([](segment_const_reference segment) -> size_t {
                return std::popcount(segment);
            },
                                   std::plus<size_t>{},
                                   0uz, full_segment());
        }

        /**
         * Sets the first bit high, and returns the corresponding index
         *
         * @return ix to first free index in bitset (if there is no free index, max_bits is returned)
         */
        [[nodiscard]] size_t set_first_free() {
            for (auto &segment : inner_) {
                auto offset = std::countr_zero(static_cast<T>(~segment));
                auto seg = std::distance(inner_.data(), &segment);

                auto global_ix = calc_global_idx(seg, offset);

                if (offset != segment_size_in_bits && global_ix < logical_size()) {
                    segment_set(seg, offset);
                    return global_ix;
                }
            }

            auto const ext_ix = logical_size();

            // can't grow in static mode
            if constexpr (!has_dynamic_extent) {
                return max_bits;
            }

            if constexpr (has_max_extent) {
                if (!fits_in_storage(ext_ix)) {
                    return max_bits;
                }
            }

            if constexpr (has_dynamic_extent) {
                expand_segments(ext_ix);
            }

            set(ext_ix);

            return ext_ix;
        }

        /**
         * Counts consecutive zeros starting from LSB
         *
         * @return ix to non-zero entry
         */
        [[nodiscard]] size_t countr_zero() const {
            auto const full_segments = full_segments_or();
            auto const full_bits = full_segments.first.size();

            auto const full_count = segments_reduce_while([](segment_const_reference segment) {
                    return std::countr_zero(segment);
                }, [](size_t const val) {
                    return val == segment_size_in_bits;
                },
                std::plus<size_t>{},
                0uz,
                full_segments.first);

            // if aligned or not fully consumed (all zeros) we can stop
            if (!full_segments.second.has_value() || full_count != full_bits) {
                return full_count;
            }

            auto const leftover = leftover_bits();
            T const &last = full_segments.second.value();
            auto const padding_mask = static_cast<T>(~T{} << leftover);
            return full_bits + static_cast<size_t>(std::countr_zero(static_cast<T>(last | padding_mask)));
        }

        /**
         * Counts consecutive zeros starting from MSB
         *
         * @return ix to non-zero entry
         */
        [[nodiscard]] size_t countl_zero() const {
            auto const leftover = leftover_bits();
            auto const full_segments = full_segments_or();

            if (full_segments.second.has_value()) {
                auto const fill_bits = segment_size_in_bits - leftover;
                T const &last = full_segments.second.value();
                auto const padding_mask = static_cast<T>(static_cast<T>(~T{}) >> (segment_size_in_bits - fill_bits));
                auto const zero_bits = static_cast<size_t>(std::countl_zero(static_cast<T>(static_cast<T>(last << fill_bits) | padding_mask)));

                if (zero_bits != leftover) {
                    return zero_bits;
                }
            }

            return segments_reduce_while_reverse([](segment_const_reference segment) {
                    return std::countl_zero(segment);
                },
                                                 [](size_t const val) {
                                                     return val == segment_size_in_bits;
                                                 },
                                                 std::plus<size_t>{},
                                                 0uz, full_segments.first) + leftover;
        }

        /**
         * Counts consecutive zeros starting from LSB
         *
         * @return ix to zero entry
         */
        [[nodiscard]] size_t countr_one() const {
            return segments_reduce_while([](segment_const_reference segment) {
                return std::countr_one(segment);
            },
                                   [](size_t const val) {
                                       return val == segment_size_in_bits;
                                   },
                                   std::plus<size_t>{},
                                   0uz, full_segment());
        }

        /**
         * Counts consecutive ones starting from MSB
         *
         * @return ix to zero entry
         */
        [[nodiscard]] size_t countl_one() const {
            return segments_reduce_while_reverse([](segment_const_reference segment) {
                return std::countl_one(segment);
            },
                                             [](size_t const val) {
                                                 return val == segment_size_in_bits;
                                             },
                                             std::plus<size_t>{},
                                             0uz, full_segment());
        }

        /**
         * Returns a bool indicating if all bits are set or not
         *
         * @return queried state
         */
        [[nodiscard]] bool all_set() const {
            auto full_segments = full_segments_or();

            auto const all_set_high = std::ranges::all_of(full_segments.first, [this](segment_const_reference segment) {
                return segment_all_set(segment);
            });

            if (!all_set_high) {
                return false;
            }

            if (!full_segments.second.has_value()) {
                return true;
            }

            return (std::popcount(full_segments.second.value()) == leftover_bits());
        }

        /**
         * Returns a bool indicating if any bit is set
         *
         * @return queried state
         */
        [[nodiscard]] bool any_set() const {
            return std::ranges::any_of(std::ranges::subrange(segments_begin(), end()), [this](segment_const_reference segment) {
                return segment_any_set(segment);
            });
        }

        /**
         * Returns a bool indicating if no bit is set
         *
         * @return queried state
         */
        [[nodiscard]] bool none_set() const {
            return std::ranges::none_of(std::ranges::subrange(segments_begin(), end()), [this](segment_const_reference segment) {
                return !segment_none_set(segment); // flip since none_of evaluates on false
            });
        }

        /**
         * Returns range whose bits are set high
         *
         * @return input range containing the positions
         */
        [[nodiscard]] std::ranges::input_range auto positions() const {
            return std::ranges::subrange(positions_begin(), positions_end());
        }

        /**
         * Set bits high on position given positions range
         *
         * @param positions input range of positions
         */
        template<typename Range>
        requires std::ranges::input_range<Range>
        void set_positions(Range &&positions) {
            std::ranges::for_each(positions, [this](auto pos) {
                this->set(pos);
            });
        }

        /**
         * Reset bits on position given positions range
         *
         * @param positions input range of positions
         */
        template<typename Range>
        requires std::ranges::input_range<Range>
        void reset_positions(Range &&positions) {
            std::ranges::for_each(positions, [this](auto pos) {
                this->reset(pos);
            });
        }

        constexpr bit_iterator begin() noexcept {
            return bit_iterator{*this};
        }

        constexpr const_bit_iterator begin() const noexcept {
            return const_bit_iterator{*this};
        }

        constexpr reverse_iterator_t rbegin() noexcept {
            return reverse_iterator_t{begin() + logical_size()};
        }

        constexpr const_reverse_iterator_t rbegin() const noexcept {
            return const_reverse_iterator_t{begin() + logical_size()};
        }

        constexpr reverse_iterator_t rend() noexcept {
            return reverse_iterator_t{begin()};
        }

        constexpr const_reverse_iterator_t rend() const noexcept {
            return const_reverse_iterator_t{begin()};
        }

        constexpr std::default_sentinel_t end() const noexcept {
            return std::default_sentinel;
        }

        constexpr positional_iterator positions_begin() noexcept {
            return positional_iterator{*this};
        }

        constexpr const_positional_iterator positions_begin() const noexcept {
            return const_positional_iterator{*this};
        }

        constexpr std::default_sentinel_t positions_end() const noexcept {
            return std::default_sentinel;
        }

    private:
        using segment_iterator = iterator<bitset_mode::SegmentMode>;
        using const_segment_iterator = const_iterator<bitset_mode::SegmentMode>;
        using segment_reverse_iterator_t = reverse_iterator<bitset_mode::SegmentMode>;
        using const_segment_reverse_iterator_t = const_reverse_iterator<bitset_mode::SegmentMode>;

        constexpr segment_iterator segments_begin() noexcept {
            return segment_iterator{*this};
        }

        constexpr const_segment_iterator segments_begin() const noexcept {
            return const_segment_iterator{*this};
        }

        constexpr segment_reverse_iterator_t segments_rbegin() noexcept {
            return segment_reverse_iterator_t{segments_begin() + size()};
        }

        constexpr const_segment_reverse_iterator_t segments_rbegin() const noexcept {
            return const_segment_reverse_iterator_t{segments_begin() + size()};
        }

        constexpr segment_reverse_iterator_t segments_rend() noexcept {
            return segment_reverse_iterator_t{segments_begin()};
        }

        constexpr const_segment_reverse_iterator_t segments_rend() const noexcept {
            return const_segment_reverse_iterator_t{segments_begin()};
        }

    public:
        /**
         * Returns total capacity bit size (rounded up)
         *
         * @return total capacity
         */
        constexpr size_t capacity_in_bits() const noexcept {
            return size() * segment_size_in_bits;
        }

        /**
         * Returns logical bit size
         *
         * @return logical bits
         */
        constexpr size_t size_in_bits() const noexcept {
            return logical_size();
        }

        bool operator==(bitset const &alt_storage) const noexcept {
            return segments_pairwise_all_of([](segment_const_reference segment_first, segment_const_reference segment_second) {
                return segment_first == segment_second;
            },
                                   alt_storage);
        }

        bitset &operator<<=(size_t shift) {
            auto dest_it = std::move_backward(begin(), begin() + logical_size() - shift, begin() + logical_size());
            std::fill(begin(), dest_it, false);
            return *this;
        }

        bitset &operator>>=(size_t shift) {
            auto dest_it = std::move(begin() + shift, begin() + logical_size(), begin());
            std::fill(dest_it, begin() + logical_size(), false);
            return *this;
        }

        bitset &operator&=(bitset const &alt_storage) {
            if (!size_match(alt_storage)) {
                throw std::logic_error("bitset:: bitset size not match");
            }

            segments_transform_with<std::bit_and<T>>(alt_storage);
            return *this;
        }

        bitset &operator|=(bitset const &alt_storage) {
            if (!size_match(alt_storage)) {
                throw std::logic_error("bitset:: bitset size not match");
            }

            segments_transform_with<std::bit_or<T>>(alt_storage);
            return *this;
        }

        bitset &operator^=(bitset const &alt_storage) {
            if (!size_match(alt_storage)) {
                throw std::logic_error("bitset:: bitset size not match");
            }

            segments_transform_with<std::bit_xor<T>>(alt_storage);
            return *this;
        }

        bitset operator<<(size_t shift) const {
            bitset tmp = *this;
            tmp <<= shift;
            return tmp;
        }

        bitset operator>>(size_t shift) const {
            bitset tmp = *this;
            tmp >>= shift;
            return tmp;
        }

        bitset operator&(bitset const &bitset_v_second) const {
            if (!size_match(bitset_v_second)) {
                throw std::logic_error("bitset:: bitset size not match");
            }

            bitset tmp = *this;
            tmp &= bitset_v_second;
            return tmp;
        }

        bitset operator|(bitset const &bitset_v_second) const {
            if (!size_match(bitset_v_second)) {
                throw std::logic_error("bitset:: bitset size not match");
            }

            bitset tmp = *this;
            tmp |= bitset_v_second;
            return tmp;
        }

        bitset operator^(bitset const &bitset_v_second) const {
            if (!size_match(bitset_v_second)) {
                throw std::logic_error("bitset:: bitset size not match");
            }

            bitset tmp = *this;
            tmp ^= bitset_v_second;
            return tmp;
        }

        bitset operator~() const {
            bitset tmp = *this;
            tmp.segments_transform<std::bit_not<T>>(tmp.full_segment());

            if (!tmp.is_aligned()) {
                auto &last = (tmp.segments_begin() + (tmp.size() - 1)).get();
                last &= static_cast<T>(static_cast<T>(~T{}) >> (segment_size_in_bits - tmp.leftover_bits()));
            }

            return tmp;
        }
    };
}  // namespace dice::template_library

template<typename T, size_t extent_, size_t max_extent_>
struct std::formatter<dice::template_library::bitset<extent_, max_extent_, T>> {
    bool binary = false;

    ///> parse formatter context, only allowing hex, debug and binary symbol
    constexpr auto parse(std::format_parse_context &ctx) {
        auto it = ctx.begin();
        while (it != ctx.end() && *it != '}') {
            if (*it != 'b') {
                throw std::format_error("Invalid format args for dice::template_library::bitset.");
            }

            if (*it == 'b') {
                binary = true;
            }
            ++it;
        }
        return it;
    }

    auto format(dice::template_library::bitset<extent_, max_extent_, T> const &storage, std::format_context &ctx) const {
        auto it = storage.begin();
        auto const end = storage.end();
        auto out = ctx.out();

        *out++ = '[';
        *out++ = '\n';

        while (it != end) {
            *out++ = '[';
            if (binary) {
                auto const bits =  static_cast<ptrdiff_t>(sizeof(T) * 8);
                for (auto const seg_end = it + bits; bool const b : std::ranges::subrange(it, seg_end) | std::views::reverse) {
                    *out++ = b ? '1' : '0';
                }
                it += bits;
            } else { // default to hex
                auto const &segment = it.get();
                out = std::format_to(out, "{:#0{}x}", segment, sizeof(segment) * 2);
                it += static_cast<ptrdiff_t>(sizeof(T) * 8);
            }
            *out++ = ']';
            *out++ = '\n';
        }
        *out++ = ']';
        *out++ = '\n';

        return out;
    }
};

#endif  //DICE_TEMPLATELIBRARY_BITSET_HPP
