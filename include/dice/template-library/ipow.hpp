#ifndef DICE_TEMPLATE_LIBRARY_IPOW_H
#define DICE_TEMPLATE_LIBRARY_IPOW_H

#include <concepts>
#include <limits>
#include <stdexcept>

namespace dice::template_library {
    /**
     * Integer power by squaring, computed in `decltype(base)` (the result has the
     * same type as `base`). Negative exponents truncate towards zero, i.e. the
     * result is 0 unless |base| == 1.
     *
     * @throws std::overflow_error if the result exceeds the maximum of the return type
     * @throws std::underflow_error if the result is below the minimum of the return type
     * @note Not supported by MVSC because it uses `__builtin_mul_overflow`
     */
    constexpr auto ipow(std::integral auto base, std::integral auto exp) {
        using ReturnType = decltype(base);

        // Handle negative exponents
        if constexpr (std::is_signed_v<decltype(exp)>) {
            if (exp < 0) {
                if (base == 1) {
                    return static_cast<ReturnType>(1);
                }
                if constexpr (std::is_signed_v<ReturnType>) {
                    if (base == -1) {
                        return static_cast<ReturnType>((exp % 2 != 0) ? -1 : 1);
                    }
                }
                return static_cast<ReturnType>(0);
            }
        }

        // multiply, trapping over-/underflow instead of invoking UB
        auto safe_mul = [base, exp](ReturnType a, ReturnType b) -> ReturnType {
            ReturnType out{};
            if (__builtin_mul_overflow(a, b, &out)) {
                if (std::cmp_less(base, 0) && (exp % 2 != 0)) {
                    throw std::underflow_error("Integer underflow detected.");
                }
                throw std::overflow_error("Integer overflow detected.");
            }
            return out;
        };

        ReturnType result = 1;
        ReturnType acc = base;

        while (exp > 0) {
            if (exp & 1) {
                result = safe_mul(result, acc);
            }

            exp >>= 1;

            // only square while another bit remains; the final unused square
            // could otherwise throw a spurious overflow
            if (exp > 0) {
                acc = safe_mul(acc, acc);
            }
        }

        return result;
    }
}

#endif //DICE_TEMPLATE_LIBRARY_IPOW_H
