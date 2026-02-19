#ifndef DICE_TEMPLATELIBRARY_DBG_HPP
#define DICE_TEMPLATELIBRARY_DBG_HPP

#include <dice/template-library/macro_util.hpp>

#include <format>
#include <iostream>
#include <iterator>
#include <utility>

/**
 * Inspired by the rust dbg!() macro.
 * Prints and returns the value of a given expression.
 *
 * The value is printed to stderr using std::format and returned as is (i.e. forwarded).
 * This is meant as a debugging tool and should not be used for debug output in production.
 * The macro also works in release builds.
 *
 * @example
 * int const a = 2;
 * int const b = DICE_DBG(a * 2) + 1;
 * //            ^ prints: [example.cpp:2] a * 2 = 4
 * assert(b == 5);
 */
#define DICE_DBG(expr)                                                                                                       \
    []<typename _dice_dbg_T>(_dice_dbg_T &&val) -> decltype(auto) {                                                          \
        std::format_to(std::ostreambuf_iterator<char>{std::cerr}, "[{}:{}] {} = {}\n", DICE_FILENAME, __LINE__, #expr, val); \
        return std::forward<_dice_dbg_T>(val);                                                                               \
    }(expr)

#endif // DICE_TEMPLATELIBRARY_DBG_HPP
