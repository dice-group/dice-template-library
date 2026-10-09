#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/optional_ref.hpp>

#include <cstddef>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dtl = dice::template_library;

namespace {
    /// `ref.emplace(arg)` compiles for an argument of type `Arg`
    template<typename Ref, typename Arg>
    concept can_emplace = requires (Ref ref, Arg &&arg) { ref.emplace(std::forward<Arg>(arg)); };

    /// `ref.and_then(f)` compiles for a function of type `F`
    template<typename Ref, typename F>
    concept can_and_then = requires (Ref ref, F &&f) { ref.and_then(std::forward<F>(f)); };

    /// `ref.value_or(u)` compiles for a default of type `U`
    template<typename Ref, typename U>
    concept can_value_or = requires (Ref ref, U &&u) { ref.value_or(std::forward<U>(u)); };

    /// `lhs == rhs` compiles
    template<typename L, typename R>
    concept can_compare = requires (L const &lhs, R const &rhs) { lhs == rhs; };

    struct base {
        int x = 0;
    };

    struct derived : base {};
}  // namespace

TEST_SUITE("optional_ref") {
    static_assert(std::is_same_v<dtl::optional_ref<int>::value_type, int>);
    static_assert(std::is_same_v<dtl::optional_ref<int const>::value_type, int const>);

    // like std::optional<T &>, optional_ref does not bind a temporary
    static_assert(std::is_constructible_v<dtl::optional_ref<int const>, int &>);
    static_assert(std::is_constructible_v<dtl::optional_ref<int const>, int const &>);
    static_assert(!std::is_constructible_v<dtl::optional_ref<int const>, int>);
    static_assert(!std::is_constructible_v<dtl::optional_ref<int const>, long>);
    static_assert(!std::is_constructible_v<dtl::optional_ref<int const>, long &>);
    static_assert(!std::is_convertible_v<int, dtl::optional_ref<int const>>);
    static_assert(!std::is_constructible_v<dtl::optional_ref<int>, int>);
    static_assert(!std::is_constructible_v<dtl::optional_ref<std::string const>, std::string const>);
    static_assert(!std::is_constructible_v<dtl::optional_ref<std::string const>, std::string>);
    static_assert(can_emplace<dtl::optional_ref<int const>, int const &>);
    static_assert(!can_emplace<dtl::optional_ref<int const>, int>);
    static_assert(!can_emplace<dtl::optional_ref<std::string const>, std::string const>);

    // optional_ref<T> converts to optional_ref<T const> and to a reference to a base class, never back
    static_assert(std::is_convertible_v<dtl::optional_ref<int>, dtl::optional_ref<int const>>);
    static_assert(!std::is_constructible_v<dtl::optional_ref<int>, dtl::optional_ref<int const>>);
    static_assert(std::is_convertible_v<dtl::optional_ref<derived>, dtl::optional_ref<base>>);
    static_assert(!std::is_constructible_v<dtl::optional_ref<derived>, dtl::optional_ref<base>>);

    // like std::optional<T &>, and_then requires a function that returns an optional, and value_or requires a
    // default that converts implicitly. std::optional<T &> rejects the other calls with a hard error in the body,
    // so the negative checks only apply to the class.
    static_assert(can_and_then<dtl::optional_ref<int>, std::optional<int> (*)(int &)>);
    static_assert(can_and_then<dtl::optional_ref<int>, dtl::optional_ref<int> (*)(int &)>);
    static_assert(can_value_or<dtl::optional_ref<std::string const>, char const *>);
#if __cpp_lib_optional < 202506L
    static_assert(!can_and_then<dtl::optional_ref<int>, bool (*)(int &)>);
    static_assert(!can_value_or<dtl::optional_ref<std::string const>, std::string_view>);
#endif

    // == only compiles if the referenced values can be compared
    static_assert(can_compare<dtl::optional_ref<int>, dtl::optional_ref<long const>>);
    static_assert(!can_compare<dtl::optional_ref<int>, dtl::optional_ref<std::string>>);

    // like std::optional<T &>, optional_ref is a view and a borrowed range, and it is not formatted as a range
    static_assert(std::ranges::view<dtl::optional_ref<int>>);
    static_assert(std::ranges::borrowed_range<dtl::optional_ref<int>>);
#if __cpp_lib_format_ranges
    static_assert(!std::formattable<dtl::optional_ref<int>, char>);
#endif

    // optional_ref is usable in constant expressions
    static_assert([] {
        int value = 1;
        dtl::optional_ref<int> ref{value};
        *ref = 2;
        return value == 2 && ref.has_value() && !dtl::optional_ref<int>{}.has_value();
    }());

    TEST_CASE("references an object or nothing") {
        std::string value = "one";
        dtl::optional_ref<std::string> ref{value};
        REQUIRE(ref.has_value());
        CHECK(static_cast<bool>(ref));
        CHECK(&*ref == &value);
        CHECK(ref.operator->() == &value);
        *ref = "uno";
        CHECK(value == "uno");

        dtl::optional_ref<std::string> const empty;
        CHECK_FALSE(empty.has_value());
        CHECK_FALSE(static_cast<bool>(empty));

        dtl::optional_ref<std::string> const from_nullopt = std::nullopt;
        CHECK_FALSE(from_nullopt.has_value());
    }

    TEST_CASE("value returns the referenced object or throws bad_optional_access") {
        int value = 1;
        dtl::optional_ref<int> const ref{value};
        CHECK(&ref.value() == &value);

        dtl::optional_ref<int> const empty;
        CHECK_THROWS_AS(static_cast<void>(empty.value()), std::bad_optional_access);
    }

    TEST_CASE("value_or returns a copy of the referenced object or the default") {
        std::string const value = "one";
        dtl::optional_ref<std::string const> const ref{value};
        dtl::optional_ref<std::string const> const empty;

        static_assert(std::is_same_v<decltype(ref.value_or("none")), std::string>);
        CHECK(ref.value_or("none") == "one");
        CHECK(empty.value_or("none") == "none");
    }

    TEST_CASE("copy assignment and emplace rebind the reference") {
        int first = 1;
        int second = 2;
        dtl::optional_ref<int> ref{first};

        ref = dtl::optional_ref<int>{second};
        REQUIRE(ref.has_value());
        CHECK(&*ref == &second);
        CHECK(first == 1);

        int &result = ref.emplace(first);
        CHECK(&result == &first);
        REQUIRE(ref.has_value());
        CHECK(&*ref == &first);
        CHECK(second == 2);

        ref = std::nullopt;
        CHECK_FALSE(ref.has_value());
        CHECK(first == 1);
    }

    TEST_CASE("reset and swap") {
        int first = 1;
        int second = 2;
        dtl::optional_ref<int> a{first};
        dtl::optional_ref<int> b{second};

        a.swap(b);
        REQUIRE(a.has_value());
        REQUIRE(b.has_value());
        CHECK(&*a == &second);
        CHECK(&*b == &first);

        dtl::optional_ref<int> empty;
        a.swap(empty);
        CHECK_FALSE(a.has_value());
        REQUIRE(empty.has_value());
        CHECK(&*empty == &second);

        b.reset();
        CHECK_FALSE(b.has_value());
        CHECK(first == 1);
        CHECK(second == 2);
    }

    TEST_CASE("transform") {
        std::string value = "one";
        dtl::optional_ref<std::string> const ref{value};
        dtl::optional_ref<std::string> const empty;

        auto const size = [](std::string const &str) {
            return str.size();
        };
        static_assert(std::is_same_v<decltype(ref.transform(size)), std::optional<std::size_t>>);
        CHECK(ref.transform(size) == std::optional<std::size_t>{3});
        CHECK_FALSE(empty.transform(size).has_value());

        // a function that returns an lvalue reference gives an optional_ref
        auto const front = [](std::string &str) -> char & {
            return str.front();
        };
        auto const front_ref = ref.transform(front);
        static_assert(std::is_same_v<decltype(front_ref), dtl::optional_ref<char> const>);
        REQUIRE(front_ref.has_value());
        CHECK(&*front_ref == &value.front());
        CHECK_FALSE(empty.transform(front).has_value());
    }

    TEST_CASE("and_then") {
        std::string value = "one";
        dtl::optional_ref<std::string> const ref{value};
        dtl::optional_ref<std::string> const empty;

        auto const size_if_not_empty = [](std::string const &str) {
            return str.empty() ? std::optional<std::size_t>{} : std::optional<std::size_t>{str.size()};
        };
        CHECK(ref.and_then(size_if_not_empty) == std::optional<std::size_t>{3});
        CHECK_FALSE(empty.and_then(size_if_not_empty).has_value());
    }

    TEST_CASE("or_else") {
        std::string value = "one";
        std::string other = "other";
        dtl::optional_ref<std::string> const ref{value};
        dtl::optional_ref<std::string> const empty;

        auto const fallback = [&other] {
            return dtl::optional_ref<std::string>{other};
        };
        auto const from_ref = ref.or_else(fallback);
        REQUIRE(from_ref.has_value());
        CHECK(&*from_ref == &value);

        auto const from_empty = empty.or_else(fallback);
        REQUIRE(from_empty.has_value());
        CHECK(&*from_empty == &other);
    }

    TEST_CASE("iteration visits the referenced object once, or nothing") {
        int value = 1;
        std::size_t visited = 0;
        for (int &element : dtl::optional_ref<int>{value}) {
            CHECK(&element == &value);
            ++visited;
        }
        CHECK(visited == 1);

        std::size_t visited_empty = 0;
        for ([[maybe_unused]] int &element : dtl::optional_ref<int>{}) {
            ++visited_empty;
        }
        CHECK(visited_empty == 0);
    }

    TEST_CASE("converts to a reference to const and to a reference to a base class") {
        int value = 1;
        dtl::optional_ref<int> const ref{value};
        dtl::optional_ref<int const> const cref = ref;
        static_assert(std::is_same_v<decltype(*cref), int const &>);
        REQUIRE(cref.has_value());
        CHECK(&*cref == &value);

        dtl::optional_ref<int const> const cempty = dtl::optional_ref<int>{};
        CHECK_FALSE(cempty.has_value());

        derived object;
        dtl::optional_ref<base> const bref = dtl::optional_ref<derived>{object};
        REQUIRE(bref.has_value());
        CHECK(&*bref == &object);
    }

    TEST_CASE("== compares the referenced values") {
        int a = 1;
        int b = 1;
        int c = 2;
        dtl::optional_ref<int> const ra{a};
        dtl::optional_ref<int const> const rb{b};
        dtl::optional_ref<int> const rc{c};
        dtl::optional_ref<int> const empty;

        // different objects with equal values
        CHECK(ra == rb);
        CHECK(rb == ra);
        CHECK(ra != rc);
        CHECK(ra != empty);
        CHECK(empty != ra);
        CHECK(empty == dtl::optional_ref<int const>{});

        CHECK(empty == std::nullopt);
        CHECK(std::nullopt == empty);
        CHECK(ra != std::nullopt);
    }
}
