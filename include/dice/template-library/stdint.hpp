#ifndef DICE_TEMPLATELIBRARY_STDINT_HPP
#define DICE_TEMPLATELIBRARY_STDINT_HPP

#include <cstddef>
#include <cstdint>

namespace dice::template_library::literals {

    constexpr uint8_t operator""_u8(unsigned long long value) noexcept {
        return static_cast<uint8_t>(value);
    }

    constexpr int8_t operator""_i8(unsigned long long value) noexcept {
        return static_cast<int8_t>(value);
    }

    constexpr uint16_t operator""_u16(unsigned long long value) noexcept {
        return static_cast<uint16_t>(value);
    }

    constexpr int16_t operator""_i16(unsigned long long value) noexcept {
        return static_cast<int16_t>(value);
    }

    constexpr uint32_t operator""_u32(unsigned long long value) noexcept {
        return static_cast<uint32_t>(value);
    }

    constexpr int32_t operator""_i32(unsigned long long value) noexcept {
        return static_cast<int32_t>(value);
    }

    constexpr uint64_t operator""_u64(unsigned long long value) noexcept {
        return static_cast<uint64_t>(value);
    }

    constexpr int64_t operator""_i64(unsigned long long value) noexcept {
        return static_cast<int64_t>(value);
    }

    constexpr size_t operator""_usize(unsigned long long value) noexcept {
        return static_cast<size_t>(value);
    }

    // Note: `ptrdiff_t` is the signed counterpart of `size_t` (https://en.cppreference.com/w/cpp/types/ptrdiff_t.html).
    // `ssize_t` is actually not standard C++. It is a posix extension.
    constexpr ptrdiff_t operator""_isize(unsigned long long value) noexcept {
        return static_cast<ptrdiff_t>(value);
    }

} // namespace dice::template_library::literals

#endif // DICE_TEMPLATELIBRARY_STDINT_HPP
