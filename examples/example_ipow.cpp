#include <dice/template-library/ipow.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>

int main() {
    using namespace dice::template_library;

    // Basic integer exponentiation, computed by squaring in integer arithmetic
    std::cout << "ipow(2, 10)  = " << ipow(2, 10) << "\n"; // 1024
    std::cout << "ipow(3, 4)   = " << ipow(3, 4) << "\n"; // 81
    std::cout << "ipow(10, 0)  = " << ipow(10, 0) << "\n"; // 1

    // The result has the same type as `base`
    static_assert(std::is_same_v<decltype(ipow(2u, 10)), unsigned int>);
    std::cout << "ipow(2u, 10) = " << ipow(2u, 10) << "\n"; // 1024 (unsigned)

    // Negative base: the sign follows the parity of the exponent
    std::cout << "ipow(-2, 3)  = " << ipow(-2, 3) << "\n"; // -8
    std::cout << "ipow(-2, 4)  = " << ipow(-2, 4) << "\n"; // 16

    // Negative exponents truncate towards zero: the result is 0 unless |base| == 1
    std::cout << "ipow(5, -1)  = " << ipow(5, -1) << "\n"; // 0
    std::cout << "ipow(1, -3)  = " << ipow(1, -3) << "\n"; // 1
    std::cout << "ipow(-1, -3) = " << ipow(-1, -3) << "\n"; // -1

    // Over- and underflow are detected and reported instead of wrapping silently
    try {
        std::cout << ipow(std::int32_t{2}, 100) << "\n";
    } catch (std::overflow_error const &e) {
        std::cout << "ipow(2, 100) threw: " << e.what() << "\n";
    }

    try {
        std::cout << ipow(std::int32_t{-3}, 21) << "\n";
    } catch (std::underflow_error const &e) {
        std::cout << "ipow(-3, 21) threw: " << e.what() << "\n";
    }
}
