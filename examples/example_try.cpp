#include <dice/template-library/ranges.hpp>
#include <dice/template-library/try_traits.hpp>

#include <array>
#include <expected>
#include <iostream>
#include <optional>
#include <string>

namespace dtl = dice::template_library;

std::expected<int, std::string> parse_int(std::string const &s) {
    try {
        return std::stoi(s);
    } catch (std::exception const &) {
        return std::unexpected{"not a number: " + s};
    }
}

// DICE_TRY unwraps the value or returns the error from the enclosing function.
// Note that the error is propagated even though the output type changes from int to double.
std::expected<double, std::string> parse_and_halve(std::string const &s) {
    int const value = DICE_TRY(parse_int(s));
    return static_cast<double>(value) / 2.0;
}

// It works for every try_result, e.g. for std::optional ...
std::optional<char> first_char_of(std::string const &s) {
    return s.empty() ? std::nullopt : std::optional{s.front()};
}

std::optional<std::string> describe_first_char(std::string const &s) {
    return std::string{"first char is '"} + DICE_TRY(first_char_of(s)) + "'";
}

// ... and for control_flow, where you pick what the break arm carries
dtl::control_flow<std::string, int> checked_double(int x) {
    if (x > 100) {
        return dtl::cfbreak{std::string{"too large"}};
    }

    return dtl::cfcontinue{x * 2};
}

dtl::control_flow<std::string, std::string> doubled_as_string(int x) {
    return dtl::cfcontinue{std::to_string(DICE_TRY(checked_double(x)))};
}

int main() {
    std::cout << "parse_and_halve(\"42\")     = " << parse_and_halve("42").value_or(-1) << "\n";
    std::cout << "parse_and_halve(\"abc\")    = " << parse_and_halve("abc").error() << "\n";

    std::cout << "describe_first_char(\"hi\") = " << describe_first_char("hi").value_or("<nothing>") << "\n";
    std::cout << "describe_first_char(\"\")   = " << describe_first_char("").value_or("<nothing>") << "\n";

    std::cout << "doubled_as_string(21)     = " << doubled_as_string(21).get_continue() << "\n";
    std::cout << "doubled_as_string(1000)   = " << doubled_as_string(1000).get_break() << "\n";

    // The same try_results drive the fallible range algorithms.
    // try_fold_left stops at the first residual and propagates it.
    std::array<std::string, 3> const numbers{"1", "2", "3"};
    auto const sum = dtl::try_fold_left(numbers, 0, [](int acc, std::string const &s) -> std::expected<int, std::string> {
        return acc + DICE_TRY(parse_int(s));
    });
    std::cout << "try_fold_left(sum)        = " << sum.value.value_or(-1) << "\n";

    std::array<std::string, 3> const broken{"1", "nope", "3"};
    auto const failed = dtl::try_fold_left(broken, 0, [](int acc, std::string const &s) -> std::expected<int, std::string> {
        return acc + DICE_TRY(parse_int(s));
    });
    std::cout << "try_fold_left(broken)     = " << failed.value.error() << "\n";
    std::cout << "  stopped at              = " << *failed.in << "\n";
}