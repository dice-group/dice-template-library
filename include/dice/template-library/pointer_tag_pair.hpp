#ifndef DICE_TEMPLATE_LIBRARY_POINTERTAGPAIR_HPP
#define DICE_TEMPLATE_LIBRARY_POINTERTAGPAIR_HPP

#include <bit>
#include <cassert>
#include <compare>
#include <cstddef>
#include <tuple>
#include <utility>

#include <dice/template-library/stdint.hpp>
#include <dice/template-library/type_traits.hpp>

namespace dice::template_library {
    namespace detail_pointer_tag_pair {
        template<typename T>
        concept taggable_pointee = !std::is_function_v<T>;

        template<typename Pointer, typename T>
        concept taggable_pointer = std::is_pointer_v<Pointer> && std::convertible_to<Pointer, T *>;

        template<typename Tag>
        concept taggable_pointer_tag = std::unsigned_integral<Tag> || (std::is_enum_v<Tag> && std::unsigned_integral<std::underlying_type_t<Tag>>);

        /**
         * The number of high bits in a pointer that can be used for tagging across all common architectures.
         * See Readme_pointer_tagging.md for details on how this number was chosen.
         */
        inline constexpr unsigned conservative_hardware_taggable_high_bits_available = 4;

        /**
         * Number of bits in a pointer
         */
        inline constexpr unsigned pointer_bits = sizeof(uintptr_t) * 8;

        template<typename T>
        consteval unsigned taggable_low_bits_available() {
            if constexpr (std::is_void_v<T>) {
                return 0; // void pointer can point to anything, therefore it can also point to a byte (which gives zero low tagging bits)
            } else {
                return std::countr_zero(alignof(T));
            }
        }

        template<unsigned Alignment>
        consteval unsigned taggable_low_bits_available() {
            return std::countr_zero(Alignment);
        }

        /**
         * Make a mask with the Len low bits set
         */
        template<unsigned Len>
        consteval uintptr_t make_low_mask() {
            using namespace dice::template_library::literals;
            return (1_uptr << Len) - 1;
        }

        /**
         * Make a mask with the Len high bits set
         */
        template<unsigned Len>
        consteval uintptr_t make_high_mask() {
            if constexpr (Len == 0) {
                return 0; // Shifting by 64 does not work even if shifting 0 << 64
            } else {
                return make_low_mask<Len>() << (pointer_bits - Len);
            }
        }

        /**
         * A mask that masks off LowBitsRequested low bits and HighBitsRequested high bits, leaving only the pointer in between.
         */
        template<unsigned LowBitsRequested, unsigned HighBitsRequested>
        consteval uintptr_t pointer_mask() {
            return ~(make_low_mask<LowBitsRequested>() | make_high_mask<HighBitsRequested>());
        }

        template<taggable_pointee T, taggable_pointer_tag Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
        [[nodiscard]] uintptr_t encode(T *ptr, Tag tag) noexcept {
            assert(std::bit_width(static_cast<uintptr_t>(tag)) <= LowBitsRequested + HighBitsRequested);
            assert((reinterpret_cast<uintptr_t>(ptr) & ~pointer_mask<LowBitsRequested, HighBitsRequested>()) == 0);

            auto const low_tag = static_cast<uintptr_t>(tag) & make_low_mask<LowBitsRequested>();

            if constexpr (HighBitsRequested == 0) {
                return reinterpret_cast<uintptr_t>(ptr) | low_tag;
            } else {
                auto const high_tag = (static_cast<uintptr_t>(tag) & (make_low_mask<HighBitsRequested>() << LowBitsRequested)) << (pointer_bits - LowBitsRequested - HighBitsRequested);
                return reinterpret_cast<uintptr_t>(ptr) | low_tag | high_tag;
            }
        }

        /**
         * Decode a pointer previously encoded via encode.
         *
         * @warning This only works in user space because we zero bits of sign-extension-bits/translation-regimen-selector; in kernel space we would need to set those bits to 1.
         */
        template<taggable_pointee T, unsigned LowBitsRequested, unsigned HighBitsRequested>
        [[nodiscard]] T *decode_pointer(uintptr_t tagged_ptr) noexcept {
            return reinterpret_cast<T *>(tagged_ptr & pointer_mask<LowBitsRequested, HighBitsRequested>());
        }

        template<taggable_pointer_tag Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
        [[nodiscard]] Tag decode_tag(uintptr_t tagged_ptr) noexcept {
            auto const low_tag = tagged_ptr & make_low_mask<LowBitsRequested>();

            if constexpr (HighBitsRequested == 0) {
                return static_cast<Tag>(low_tag);
            } else {
                auto const high_tag = (tagged_ptr & make_high_mask<HighBitsRequested>()) >> (pointer_bits - HighBitsRequested - LowBitsRequested);
                return static_cast<Tag>(low_tag | high_tag);
            }
        }
    } // namespace detail_pointer_tag_pair

    /**
     * Implementation of the upcoming standard pointer_tag_pair with the additional feature of also supporting tagging the most significant bits.
     * By default, tagging the MSB is disabled, to opt in the last template parameter must be set.
     * Opting-in allows a larger tag (comprised of the bits packed into the LSBs and MSBs).
     */
    template<detail_pointer_tag_pair::taggable_pointee T,
             detail_pointer_tag_pair::taggable_pointer_tag Tag,
             unsigned LowBitsRequested = detail_pointer_tag_pair::taggable_low_bits_available<T>(),
             unsigned HighBitsRequested = 0>
    struct pointer_tag_pair {
        static_assert(LowBitsRequested + HighBitsRequested <= detail_pointer_tag_pair::pointer_bits,
                      "Requested more bits than can physically fit into the pointer");
    private:
        uintptr_t value_ = 0;

        explicit constexpr pointer_tag_pair(uintptr_t const value) noexcept
            : value_{value} {
        }

    public:
        using pointer_type = T *;
        using tagged_pointer_type = copy_cv_t<T, void> *;
        using tag_type = Tag;

        static constexpr unsigned bits_requested = LowBitsRequested + HighBitsRequested;

        constexpr pointer_tag_pair() noexcept = default;

        constexpr pointer_tag_pair(std::nullptr_t, Tag const tag = Tag{}) noexcept
            : pointer_tag_pair{detail_pointer_tag_pair::encode<T, Tag, LowBitsRequested, HighBitsRequested>(nullptr, tag)} {
        }

        template<detail_pointer_tag_pair::taggable_pointer<T> P>
        requires (LowBitsRequested <= detail_pointer_tag_pair::taggable_low_bits_available<std::remove_pointer_t<P>>()
                  && HighBitsRequested <= detail_pointer_tag_pair::conservative_hardware_taggable_high_bits_available)
        pointer_tag_pair(P const pointer, Tag const tag) noexcept
            : pointer_tag_pair{detail_pointer_tag_pair::encode<T, Tag, LowBitsRequested, HighBitsRequested>(pointer, tag)} {
        }

        /**
         * Tag a pointer where the known runtime alignment exceeds the type's inherent alignment (e.g. via alignas on variable decl).
         *
         * @tparam PromisedAlignment the actual alignment of the object
         * @param pointer overaligned pointer with at least alignment of PromisedAlignment
         * @param tag tag to encode
         * @return pointer_tag_pair
         *
         * @pre pointer must be aligned with at least PromisedAlignment
         */
        template<unsigned PromisedAlignment, detail_pointer_tag_pair::taggable_pointer<T> P>
        requires (LowBitsRequested <= detail_pointer_tag_pair::taggable_low_bits_available<PromisedAlignment>()
                  && HighBitsRequested <= detail_pointer_tag_pair::conservative_hardware_taggable_high_bits_available)
        [[nodiscard]] static pointer_tag_pair from_overaligned(P const pointer, Tag const tag) noexcept {
            return pointer_tag_pair{detail_pointer_tag_pair::encode<T, Tag, LowBitsRequested, HighBitsRequested>(pointer, tag)};
        }

        /**
         * Tag a pointer on a specific hardware where you know that the conservative_hardware_taggable_high_bits_available limit
         * can be safely exceeded (e.g. on x86-64).
         *
         * @param pointer pointer to tag
         * @param tag tag to encode
         * @return pointer_tag_pair
         *
         * @pre the specific hardware must not use the additional high bits
         * @pre the specific hardware must either have TBI (top-byte-ignore or equivalent) or the application must run in user-space
         */
        template<detail_pointer_tag_pair::taggable_pointer<T> P>
        requires (LowBitsRequested <= detail_pointer_tag_pair::taggable_low_bits_available<std::remove_pointer_t<P>>())
        [[nodiscard]] static pointer_tag_pair from_specific_hardware(P const pointer, Tag const tag) noexcept {
            return pointer_tag_pair{detail_pointer_tag_pair::encode<T, Tag, LowBitsRequested, HighBitsRequested>(pointer, tag)};
        }

        /**
         * Combination of from_overaligned and from_specific_hardware.
         * Both the regular LSB and MSB caps can be exceeded.
         *
         * @param pointer overaligned pointer with at least alignment of PromisedAlignment
         * @param tag tag to encode
         * @return pointer_tag_pair
         *
         * @pre pointer must be aligned with at least PromisedAlignment
         * @pre the specific hardware must not use the additional high bits
         * @pre the specific hardware must either have TBI (top-byte-ignore or equivalent) or the application must run in user-space
         */
        template<unsigned PromisedAlignment, detail_pointer_tag_pair::taggable_pointer<T> P>
        requires (LowBitsRequested <= detail_pointer_tag_pair::taggable_low_bits_available<PromisedAlignment>())
        [[nodiscard]] static pointer_tag_pair from_overaligned_specific_hardware(P const pointer, Tag const tag) noexcept {
            return pointer_tag_pair{detail_pointer_tag_pair::encode<T, Tag, LowBitsRequested, HighBitsRequested>(pointer, tag)};
        }

        /**
         * Construct from an already tagged pointer
         */
        [[nodiscard]] static pointer_tag_pair from_tagged(tagged_pointer_type const pointer) noexcept {
            return pointer_tag_pair{reinterpret_cast<uintptr_t>(pointer)};
        }

        [[nodiscard]] tagged_pointer_type tagged_pointer() const noexcept {
            return reinterpret_cast<tagged_pointer_type>(value_);
        }

        [[nodiscard]] T *pointer() const noexcept {
            return detail_pointer_tag_pair::decode_pointer<T, LowBitsRequested, HighBitsRequested>(value_);
        }

        [[nodiscard]] constexpr Tag tag() const noexcept {
            return detail_pointer_tag_pair::decode_tag<Tag, LowBitsRequested, HighBitsRequested>(value_);
        }

        friend constexpr void swap(pointer_tag_pair &lhs, pointer_tag_pair &rhs) noexcept {
            std::swap(lhs.value_, rhs.value_);
        }

        constexpr std::strong_ordering operator<=>(pointer_tag_pair const &other) const noexcept = default;
    };

    template<size_t I, typename T, typename Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
    constexpr std::tuple_element_t<I, pointer_tag_pair<T, Tag, LowBitsRequested, HighBitsRequested>>
    get(pointer_tag_pair<T, Tag, LowBitsRequested, HighBitsRequested> const pointer) {
        if constexpr (I == 0) {
            return pointer.pointer();
        } else {
            return pointer.tag();
        }
    }
} // namespace dice::template_library

template<typename T, typename Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
struct std::tuple_size<dice::template_library::pointer_tag_pair<T, Tag, LowBitsRequested, HighBitsRequested> > {
    static constexpr size_t value = 2;
};

template<typename T, typename Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
struct std::tuple_size<dice::template_library::pointer_tag_pair<T, Tag, LowBitsRequested, HighBitsRequested> const> {
    static constexpr size_t value = 2;
};

template<typename T, typename Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
struct std::tuple_element<0, dice::template_library::pointer_tag_pair<T, Tag, LowBitsRequested, HighBitsRequested> > {
    using type = T *;
};

template<typename T, typename Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
struct std::tuple_element<1, dice::template_library::pointer_tag_pair<T, Tag, LowBitsRequested, HighBitsRequested> > {
    using type = Tag;
};

template<typename T, typename Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
struct std::tuple_element<0, dice::template_library::pointer_tag_pair<T, Tag, LowBitsRequested, HighBitsRequested> const> {
    using type = T *;
};

template<typename T, typename Tag, unsigned LowBitsRequested, unsigned HighBitsRequested>
struct std::tuple_element<1, dice::template_library::pointer_tag_pair<T, Tag, LowBitsRequested, HighBitsRequested> const> {
    using type = Tag;
};

#endif // DICE_TEMPLATE_LIBRARY_POINTERTAGPAIR_HPP
