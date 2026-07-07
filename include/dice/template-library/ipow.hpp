#ifndef DICE_TEMPLATE_LIBRARY_IPOW_H
#define DICE_TEMPLATE_LIBRARY_IPOW_H

#include <concepts>
#include <limits>
#include <stdexcept>

namespace dice::template_library::math {
    constexpr auto ipow(std::integral auto base, std::integral auto exp) {
            using ReturnType = decltype(base);

            // Handle negative exponents
            if (exp < 0) {
                if (base == 1) {
                    return static_cast<ReturnType>(1);
                }
                if (base == -1) {
                    return static_cast<ReturnType>((exp % 2 != 0) ? -1 : 1);
                }
                return static_cast<ReturnType>(0);
            }

            // Helper lambda to safely multiply two integers without triggering UB
            auto safe_mul = [](ReturnType a, ReturnType b) -> ReturnType {
                if (a == 0 || b == 0) {
                    return 0;
                }

                if constexpr (std::is_signed_v<ReturnType>) {
                    // Signed integer checks
                    if (a > 0 && b > 0 && a > std::numeric_limits<ReturnType>::max() / b) {
                        throw std::overflow_error("Integer overflow detected.");
                    }
                    if (a > 0 && b < 0 && b < std::numeric_limits<ReturnType>::min() / a) {
                        throw std::underflow_error("Integer underflow detected.");
                    }
                    if (a < 0 && b > 0 && a < std::numeric_limits<ReturnType>::min() / b) {
                        throw std::underflow_error("Integer underflow detected.");
                    }
                    if (a < 0 && b < 0 && a < std::numeric_limits<ReturnType>::max() / b) {
                        throw std::overflow_error("Integer overflow detected.");
                    }
                } else {
                    // Unsigned integer check (only overflow is possible)
                    if (a > std::numeric_limits<ReturnType>::max() / b) {
                        throw std::overflow_error("Integer overflow detected.");
                    }
                }
                return a * b;
            };

            ReturnType result = 1;

            while (exp > 0) {
                if (exp & 1) {
                    result = safe_mul(result, base);  // Multiply to result safely
                }

                exp >>= 1;

                // CRITICAL OPTIMIZATION:
                // Only square the base if we have remaining exponent bits to process.
                // This prevents throwing a spurious overflow exception on the final iteration
                // when the algorithm calculates a base square it will never actually use.
                if (exp > 0) {
                    base = safe_mul(base, base);
                }
            }

            return result;
        }
}

#endif //DICE_TEMPLATE_LIBRARY_IPOW_H
