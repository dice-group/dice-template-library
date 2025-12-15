#ifndef DICE_TEMPLATELIBRARY_FORMAT_TO_OSTREAM
#define DICE_TEMPLATELIBRARY_FORMAT_TO_OSTREAM

/*!
 * Note: This file does not use concepts/requires anywhere because their evaluation
 * causes an infinite loop (compiler error: "evaluation of constraint depends on itself").
 *
 * Old-school SFINAE does not have this issue.
 */

#include <format>
#include <iterator>
#include <ostream>
#include <string>
#include <type_traits>

namespace dice::template_library {

    /**
     * Detect if a type can be used with operator<< of an ostream.
     *
     * @tparam T type to check
     * @tparam Char char-type of the ostream
     * @tparam CharTraits char-traits-type of the ostream
     */
    template<typename T, typename Char, typename CharTraits, typename = void>
    struct is_ostreamable : std::false_type {
    };

    template<typename T, typename Char, typename CharTraits>
    struct is_ostreamable<T, Char, CharTraits, std::void_t<decltype(std::declval<std::basic_ostream<Char, CharTraits> &>() << std::declval<T const &>())>> : std::true_type {
    };

    template<typename T, typename Char, typename CharTraits>
    inline constexpr bool is_ostreamable_v = is_ostreamable<T, Char, CharTraits>::value;

} // namespace dice::template_library

/**
 * An overload for ostream operator<< for any type that is formattable with std::format
 *
 * @tparam Char char-type of the ostream and the formatter
 * @tparam CharTraits char-traits-type of the ostream
 * @param stream ostream
 * @param value value to stream via std::formatter implementation
 * @return stream
 */
template<typename T, typename Char = char, typename CharTraits = std::char_traits<char>, typename = std::enable_if_t<std::formattable<T, Char> && !dice::template_library::is_ostreamable_v<T, Char, CharTraits>>>
std::basic_ostream<Char, CharTraits> &operator<<(std::basic_ostream<Char, CharTraits> &stream, T const &value) {
    std::format_to(std::ostreambuf_iterator<Char, CharTraits>{stream}, "{}", value);
    return stream;
}

#endif // DICE_TEMPLATELIBRARY_FORMAT_TO_OSTREAM
