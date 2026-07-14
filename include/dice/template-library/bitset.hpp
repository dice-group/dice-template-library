#ifndef DICE_TEMPLATELIBRARY_BITSET_HPP
#define DICE_TEMPLATELIBRARY_BITSET_HPP

#include <dice/template-library/flex_array.hpp>
#include <bit>
#include <functional>
#include <numeric>

namespace dice::template_library {

    template<typename Tp>
        struct MergeFunctor {
        Tp operator()(Tp const v1, Tp const v2) const noexcept {
            return v1 + v2;
        };
    };

    template<>
    struct MergeFunctor<bool> {
        bool operator()(bool const v1, bool const v2) const {
            return v1 & v2;
        }
    };

    template<typename T, size_t extent, size_t segments>
    struct bitset {
        static constexpr bool   has_storage_limit = segments != dynamic_extent;
        static constexpr size_t segment_size = sizeof(T);
        static constexpr size_t segment_size_in_bits = segment_size * 8;
        static constexpr size_t storage_size = !has_storage_limit ? dynamic_extent : segment_size * segments;
        static constexpr size_t storage_size_in_bits = !has_storage_limit ? dynamic_extent : storage_size * 8;

        using storage   = flex_array<T, extent, segments>;
        using global_ix = size_t;
        using segment   = size_t;
        using offset    = size_t;

    private:
        struct bitset_iterator {
        private:
            storage::pointer cur_segment_;
            offset          cur_offset_{};
            bitset *const backing_bitset_;

        public:
            explicit bitset_iterator(storage &bitset) noexcept :
                cur_segment_{bitset.data()}, backing_bitset_{&bitset} {}

            explicit bitset_iterator(storage &bitset, offset const& o) :
                cur_segment_{bitset.data()}, backing_bitset_{&bitset}{
                if constexpr (o >= segment_size) {
                    throw std::out_of_range{"bitset_iterator: o >= segment_size"};
                }
                cur_offset_ = o;
            }

            explicit bitset_iterator(storage &bitset, offset const& o, segment const& s) :
                backing_bitset_{&bitset}{
                if constexpr (o >= segment_size) {
                    throw std::out_of_range{"bitset_iterator: o >= segment_size"};
                }

                if constexpr (cur_segment_ + s >= bitset.size()) {
                    throw std::out_of_range{"bitset_iterator: segment out of bounds"};
                }

                cur_offset_ = o;
                cur_segment_ = bitset.data() + s;
            }

            void operator=(bool const b) const noexcept {
                if (b) {
                    backing_bitset_->set(std::distance(cur_segment_, backing_bitset_->inner_.data()), cur_offset_);
                    return;
                }
                backing_bitset_->unset(std::distance(cur_segment_, backing_bitset_->inner_.data()), cur_offset_);
            }

            bool operator*() const noexcept {
                return backing_bitset_->test(std::distance(cur_segment_, backing_bitset_->inner_.data()), cur_offset_);
            }

            bitset_iterator& operator++(int) noexcept {
                if (++cur_offset_ >= segment_size_in_bits) {
                    ++cur_segment_;
                    cur_offset_ = 0;
                    return *this;
                }
                return *this;
            }

            bool consumed() const noexcept {
                return cur_segment_ >= backing_bitset_;
            }

            friend bool operator==(std::default_sentinel_t, bitset_iterator const& it) {
                return it == std::default_sentinel_t{};
            }

            friend bool operator==(bitset_iterator const& it, std::default_sentinel_t) {
                return it.consumed();
            }
        };

        struct AutoModeTag {};
        struct DefaultModeTag {};

        storage inner_;

        [[nodiscard]] constexpr size_t require_segments(global_ix const ix) const requires (!has_storage_limit) {
            auto const bit_pos = bits_consumed();
            if (bit_pos >= ix) {
                return 0;
            }

            auto const bit_off = bit_pos - ix;

            return [bit_off] {
                if constexpr (std::has_single_bit(segment_size_in_bits)) {
                    return bit_off >> std::countr_zero(segment_size_in_bits);
                }
                else {
                    return bit_off / segment_size_in_bits;
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

        [[nodiscard]] static constexpr global_ix calc_global_idx(segment const s, offset const o) {
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
        auto bitset_op_cntl(F &&ops) -> std::invoke_result_t<F, typename storage::const_reference> {
            using result_t = std::invoke_result_t<F, bitset*>;

            if constexpr (std::is_void_v<result_t>) {
                std::invoke(std::forward<F>(ops), this);
                return;
            }
            else {
                return std::invoke(std::forward<F>(ops), this);
            }
        }

        void set(segment const s, offset const o) {
            *(inner_.data() + s) |= 1uz << o;
        }

        void flip(segment const s, offset const o) {
            *(inner_.data() + s) ^= 1uz << o;
        }

        void unset(segment const s, offset const o) {
            *(inner_.data() + s) &= ~(1uz << o);
        }

        [[nodiscard]] bool test(segment const s, offset const o) {
            return *(inner_.data() + s) & 1uz << o;
        }

        [[nodiscard]] size_t count(storage::const_reference segment) {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::popcount(segment);
            }

            auto byte = reinterpret_cast<const std::uint8_t*>(&segment);
            auto const end = byte + segment_size;

            auto accumulated{0uz};
            for (; byte != end; ++byte) {
                accumulated += std::popcount(*byte);
            }

            return accumulated;
        }

#ifdef __SIZEOF_INT128__
        size_t count(__uint128_t const& segment) {
            uint64_t const lo = static_cast<uint64_t>(segment);
            uint64_t const hi = static_cast<uint64_t>(segment >> 64);

            return std::popcount(lo) + std::popcount(hi);
        }
#endif

        [[nodiscard]] size_t segment_free(storage::const_reference segment) {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::countr_zero(~segment);
            }

            auto byte = reinterpret_cast<const std::uint8_t*>(&segment);
            auto const end = byte + segment_size;

            for (; byte != end; ++byte) {
                if (auto const segment_free = static_cast<uint8_t>(~(*byte)); segment_free != 0) {
                    return std::countr_zero(segment_free);
                }
            }

            return segment_size_in_bits;
        }

        [[nodiscard]] size_t segment_countl_zero(storage::const_reference segment) const noexcept {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::countl_zero(segment);
            }
        }

        [[nodiscard]] size_t segment_countr_zero(storage::const_reference segment) const noexcept {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::countr_zero(segment);
            }
        }

        [[nodiscard]] bool all_set(storage::const_reference segment) const noexcept {
            return segment_free(segment) == segment_size_in_bits;
        }

        [[nodiscard]] bool any_set(storage::const_reference segment) const noexcept {
            auto const free = segment_free(segment);

            return free > 0 && free < segment_size_in_bits;
        }

        [[nodiscard]] bool none_set(storage::const_reference segment) const noexcept {
            return segment_free(segment) == 0x00;
        }

    public:
        using iterator = bitset_iterator;

        void set(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::set, ix);
        }

        void flip(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::flip, ix);
        }

        void unset(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::unset, ix);
        }

        [[nodiscard]] bool test(global_ix const ix) {
            return bitset_mod_cntl(AutoModeTag{}, &bitset::test, ix);
        }

        [[nodiscard]] size_t count() {
            size_t accumulated{0};
            for (auto const &segment : inner_) {
                accumulated += bitset_op_cntl(&bitset::count, segment);
            }

            return accumulated;
        }

        [[nodiscard]] size_t set_first_free() {
            for (auto const &segment : inner_) {
                auto offset = bitset_op_cntl(&bitset::segment_free, segment);

                if (offset != segment_size_in_bits) {
                    auto seg = std::distance(inner_.data(), &segment);
                    set(seg, offset);
                    return calc_global_idx(seg, offset);
                }
            }

            if constexpr (storage::has_dynamic_extent) {
                if constexpr (!has_storage_limit) {
                    inner_.resize(inner_.size() + 1);
                    *static_cast<uint8_t*>(inner_.end() - 1) = 0x01;

                    return calc_global_idx(std::distance(inner_.end(), inner_.data(), 0));
                }
                else {
                    if (inner_.size() == inner_.max_size()) {
                        return storage_size_in_bits;
                    }
                    *static_cast<uint8_t*>(inner_.end()) = 0x01;
                    inner_.resize(inner_.size() + 1);

                    return calc_global_idx(std::distance(inner_.end(), inner_.data()), 0);
                }
            }

            return storage_size_in_bits;
        }

        template<typename F, typename M, typename Tp>
        requires std::is_same_v<std::invoke_result_t<F, typename storage::const_reference>, Tp> &&
            std::invocable<M, Tp, Tp>
        Tp segment_handler(F &&handler, M &&merge, Tp initial) {
            Tp merge_val{initial};
            for (auto const &segment : inner_) {
                std::invoke(std::forward<M>(merge)(
                    merge_val,
                    std::invoke(std::forward<F>(handler), segment)));
            }

            return merge_val;
        }

        [[nodiscard]] size_t countr_zero() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::countr_zero, segment);
            }, MergeFunctor<size_t>{}, 0);
        }

        [[nodiscard]] size_t countl_zero() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::countl_zero, segment);
            }, MergeFunctor<size_t>{}, 0);
        }

        [[nodiscard]] size_t countr_one() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::countr_one, segment);
            }, MergeFunctor<size_t>{}, 0);
        }

        [[nodiscard]] size_t countl_one() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::countl_one, segment);
            }, MergeFunctor<size_t>{}, 0);
        }

        [[nodiscard]] bool all_set() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::all_set, segment);
            }, MergeFunctor<bool>{}, true);
        }

        [[nodiscard]] bool any_set() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::any_set, segment);
            }, MergeFunctor<bool>{}, true);
        }

        [[nodiscard]] bool none_set() const {
            return segment_handler([this](typename storage::const_reference segment) {
                return bitset_op_cntl(&bitset::none_set, segment);
            }, MergeFunctor<bool>{}, true);
        }

        constexpr iterator begin() {
            return iterator{*this};
        }

        constexpr std::default_sentinel_t end() {
            return std::default_sentinel_t{};
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
};

#endif //DICE_TEMPLATELIBRARY_BITSET_HPP
