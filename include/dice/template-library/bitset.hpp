#ifndef DICE_TEMPLATELIBRARY_BITSET_HPP
#define DICE_TEMPLATELIBRARY_BITSET_HPP

#include <dice/template-library/flex_array.hpp>
#include <bit>
#include <functional>
#include <numeric>

namespace dice::template_library {

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
        auto bitset_op_cntl(F&& ops) -> std::invoke_result_t<F, typename storage::const_reference> {
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

        bool test(segment const s, offset const o) {
            return *(inner_.data() + s) & 1uz << o;
        }

        size_t count(storage::const_reference segment) {
            if constexpr (std::integral<typename storage::value_type>) {
                return std::popcount(segment);
            }

            auto const bytes = reinterpret_cast<const std::uint8_t*>(&segment);

            return std::accumulate(bytes, bytes + segment_size, 0);
        }

#ifdef __SIZEOF_INT128__
        size_t count(__uint128_t const& segment) {
            uint64_t const lo = static_cast<uint64_t>(segment);
            uint64_t const hi = static_cast<uint64_t>(segment >> 64);

            return std::popcount(lo) + std::popcount(hi);
        }
#endif

    public:
        void set(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::set, ix);
        }

        void flip(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::flip, ix);
        }

        void unset(global_ix const ix) {
            bitset_mod_cntl(AutoModeTag{}, &bitset::unset, ix);
        }

        bool test(global_ix const ix) {
            return bitset_mod_cntl(AutoModeTag{}, &bitset::test, ix);
        }

        size_t count() {
            size_t accumulated{0};
            for (auto const &segment : inner_) {
                accumulated += bitset_op_cntl(&bitset::count, segment);
            }
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
