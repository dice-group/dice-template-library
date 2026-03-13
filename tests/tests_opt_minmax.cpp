#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/opt_minmax.hpp>

#include <doctest/doctest.h>

#include <vector>

namespace dice::template_library {
    TEST_SUITE("opt_minmax") {
        TEST_CASE("opt_min") {
            SUBCASE("mixed plain and optional") {
                auto result = opt_min<std::optional<int>>({10, std::optional{3}, 5});
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("all nullopt") {
                auto result = opt_min(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("basic comparison") {
                auto result = opt_min<int>({5, 3, 8, 1, 4});
                REQUIRE(result.has_value());
                CHECK(*result == 1);
            }

            SUBCASE("negative numbers") {
                auto result = opt_min<int>({-5, 3, -8, 1});
                REQUIRE(result.has_value());
                CHECK(*result == -8);
            }

            SUBCASE("all same values") {
                auto result = opt_min<int>({4, 4, 4});
                REQUIRE(result.has_value());
                CHECK(*result == 4);
            }

            SUBCASE("2-mix: plain, plain") {
                auto result = opt_min<int>(5, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: plain, empty optional") {
                auto result = opt_min<int>(5, std::optional<int>{});
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: plain, nullopt") {
                auto result = opt_min<int>(5, std::nullopt);
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: empty optional, plain") {
                auto result = opt_min<int>(std::optional<int>{}, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: empty optional, empty optional") {
                auto result = opt_min(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: empty optional, nullopt") {
                auto result = opt_min<int>(std::optional<int>{}, std::nullopt);
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, plain") {
                auto result = opt_min<int>(std::nullopt, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: nullopt, empty optional") {
                auto result = opt_min<int>(std::nullopt, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, nullopt") {
                auto result = opt_min<int>(std::nullopt, std::nullopt);
                CHECK(!result.has_value());
            }

            SUBCASE("typed: explicit T with plain args") {
                auto result = opt_min<double>({5, 3, 8});
                REQUIRE(result.has_value());
                CHECK(*result == 3.0);
            }

            SUBCASE("typed: explicit T with mixed args") {
                auto result = opt_min<int>({10, std::optional{3}, std::nullopt});
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("typed: explicit T all nullopt") {
                auto result = opt_min<int>(std::nullopt, std::optional<int>{});
                CHECK(!result.has_value());
            }
        }

        TEST_CASE("opt_max") {
            SUBCASE("mixed plain and optional") {
                auto result = opt_max<int>({10, std::optional{3}, 5});
                REQUIRE(result.has_value());
                CHECK(*result == 10);
            }

            SUBCASE("all nullopt") {
                auto result = opt_max(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("basic comparison") {
                auto result = opt_max<int>({5, 3, 8, 1, 4});
                REQUIRE(result.has_value());
                CHECK(*result == 8);
            }

            SUBCASE("negative numbers") {
                auto result = opt_max<int>({-5, 3, -8, 1});
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("all same values") {
                auto result = opt_max<int>({4, 4, 4});
                REQUIRE(result.has_value());
                CHECK(*result == 4);
            }

            SUBCASE("2-mix: plain, plain") {
                auto result = opt_max<int>(3, 5);
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: plain, empty optional") {
                auto result = opt_max<int>(5, std::optional<int>{});
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: plain, nullopt") {
                auto result = opt_max<int>(5, std::nullopt);
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: empty optional, plain") {
                auto result = opt_max<int>(std::optional<int>{}, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: empty optional, empty optional") {
                auto result = opt_max(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: empty optional, nullopt") {
                auto result = opt_max<int>(std::optional<int>{}, std::nullopt);
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, plain") {
                auto result = opt_max<int>(std::nullopt, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: nullopt, empty optional") {
                auto result = opt_max<int>(std::nullopt, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, nullopt") {
                auto result = opt_max<int>(std::nullopt, std::nullopt);
                CHECK(!result.has_value());
            }

            SUBCASE("typed: explicit T with plain args") {
                auto result = opt_max<double>({5, 3, 8});
                REQUIRE(result.has_value());
                CHECK(*result == 8.0);
            }

            SUBCASE("typed: explicit T with mixed args") {
                auto result = opt_max<int>({1, std::optional{10}, std::nullopt});
                REQUIRE(result.has_value());
                CHECK(*result == 10);
            }

            SUBCASE("typed: explicit T all nullopt") {
                auto result = opt_max<int>(std::nullopt, std::optional<int>{});
                CHECK(!result.has_value());
            }
        }

        TEST_CASE("opt_minmax") {
            SUBCASE("multiple values") {
                auto result = opt_minmax<int>({5, 3, 8, 1, 4});
                REQUIRE(result.has_value());
                CHECK(result->min == 1);
                CHECK(result->max == 8);
            }

            SUBCASE("all nullopt") {
                auto result = opt_minmax(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("mixed plain and optional") {
                auto result = opt_minmax<int>({10, std::optional<int>{}, std::optional{3}, 5});
                REQUIRE(result.has_value());
                CHECK(result->min == 3);
                CHECK(result->max == 10);
            }

            SUBCASE("2-mix: plain, plain") {
                auto result = opt_minmax<int>(3, 5);
                REQUIRE(result.has_value());
                CHECK(result->min == 3);
                CHECK(result->max == 5);
            }

            SUBCASE("2-mix: plain, empty optional") {
                auto result = opt_minmax<int>(5, std::optional<int>{});
                REQUIRE(result.has_value());
                CHECK(result->min == 5);
                CHECK(result->max == 5);
            }

            SUBCASE("2-mix: plain, nullopt") {
                auto result = opt_minmax<int>(5, std::nullopt);
                REQUIRE(result.has_value());
                CHECK(result->min == 5);
                CHECK(result->max == 5);
            }

            SUBCASE("2-mix: empty optional, plain") {
                auto result = opt_minmax<int>(std::optional<int>{}, 3);
                REQUIRE(result.has_value());
                CHECK(result->min == 3);
                CHECK(result->max == 3);
            }

            SUBCASE("2-mix: empty optional, empty optional") {
                auto result = opt_minmax(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: empty optional, nullopt") {
                auto result = opt_minmax<int>(std::optional<int>{}, std::nullopt);
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, plain") {
                auto result = opt_minmax<int>(std::nullopt, 3);
                REQUIRE(result.has_value());
                CHECK(result->min == 3);
                CHECK(result->max == 3);
            }

            SUBCASE("2-mix: nullopt, empty optional") {
                auto result = opt_minmax<int>(std::nullopt, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, nullopt") {
                auto result = opt_minmax<int>(std::nullopt, std::nullopt);
                CHECK(!result.has_value());
            }

            SUBCASE("typed: explicit T with plain args") {
                auto result = opt_minmax<double>({5, 3, 8});
                REQUIRE(result.has_value());
                CHECK(result->min == 3.0);
                CHECK(result->max == 8.0);
            }

            SUBCASE("typed: explicit T with mixed args") {
                auto result = opt_minmax<int>({1, std::optional{10}, std::nullopt});
                REQUIRE(result.has_value());
                CHECK(result->min == 1);
                CHECK(result->max == 10);
            }

            SUBCASE("typed: explicit T all nullopt") {
                auto result = opt_minmax<int>(std::nullopt, std::optional<int>{});
                CHECK(!result.has_value());
            }
        }

        TEST_CASE("opt_min_element") {
            SUBCASE("empty range") {
                std::vector<int> v;
                auto result = opt_min_element(v);
                CHECK(!result.has_value());
            }

            SUBCASE("single element") {
                std::vector<int> v{42};
                auto result = opt_min_element(v);
                REQUIRE(result.has_value());
                CHECK(*result == 42);
            }

            SUBCASE("multiple elements") {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_min_element(v);
                REQUIRE(result.has_value());
                CHECK(*result == 1);
            }

            SUBCASE("range of optionals") {
                std::vector<std::optional<int>> v{std::nullopt, std::optional{5}, std::optional{2}, std::nullopt};
                auto result = opt_min_element(v);
                REQUIRE(result.has_value());
                CHECK(*result == 2);
            }

            SUBCASE("range of all nullopt optionals") {
                std::vector<std::optional<int>> v{std::nullopt, std::nullopt};
                auto result = opt_min_element(v);
                CHECK(!result.has_value());
            }
        }

        TEST_CASE("opt_max_element") {
            SUBCASE("empty range") {
                std::vector<int> v;
                auto result = opt_max_element(v);
                CHECK(!result.has_value());
            }

            SUBCASE("single element") {
                std::vector<int> v{42};
                auto result = opt_max_element(v);
                REQUIRE(result.has_value());
                CHECK(*result == 42);
            }

            SUBCASE("multiple elements") {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_max_element(v);
                REQUIRE(result.has_value());
                CHECK(*result == 8);
            }

            SUBCASE("range of optionals") {
                std::vector<std::optional<int>> v{std::nullopt, std::optional{5}, std::optional{2}, std::nullopt};
                auto result = opt_max_element(v);
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }
        }

        TEST_CASE("opt_minmax_element") {
            SUBCASE("empty range") {
                std::vector<int> v;
                auto result = opt_minmax_element(v);
                CHECK(!result.has_value());
            }

            SUBCASE("single element") {
                std::vector<int> v{42};
                auto result = opt_minmax_element(v);
                REQUIRE(result.has_value());
                CHECK(result->min == 42);
                CHECK(result->max == 42);
            }

            SUBCASE("multiple elements") {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_minmax_element(v);
                REQUIRE(result.has_value());
                CHECK(result->min == 1);
                CHECK(result->max == 8);
            }

            SUBCASE("range of optionals") {
                std::vector<std::optional<int>> v{
                    std::nullopt,
                    std::optional{5},
                    std::optional{2},
                    std::nullopt,
                    std::optional{9}};
                auto result = opt_minmax_element(v);
                REQUIRE(result.has_value());
                CHECK(result->min == 2);
                CHECK(result->max == 9);
            }
        }

        TEST_CASE("opt_min with custom comparator") {
            SUBCASE("variadic with std::greater reverses order") {
                auto result = opt_min<int>({5, 3, 8, 1, 4}, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 8);
            }

            SUBCASE("variadic with lambda") {
                auto result = opt_min<int>({5, 3, 8}, [](int a, int b) {
                    return a > b;
                });
                REQUIRE(result.has_value());
                CHECK(*result == 8);
            }

            SUBCASE("variadic with mixed args and comp") {
                auto result = opt_min<int>({10, std::optional{3}, std::nullopt}, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 10);
            }

            SUBCASE("variadic typed with comp") {
                auto result = opt_min<double>({5, 3, 8}, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 8.0);
            }

            SUBCASE("range with std::greater") {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_min_element(v, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 8);
            }

            SUBCASE("range of optionals with comp") {
                std::vector<std::optional<int>> v{std::nullopt, std::optional{5}, std::optional{2}};
                auto result = opt_min_element(v, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }
        }

        TEST_CASE("opt_max with custom comparator") {
            SUBCASE("variadic with std::greater reverses order") {
                auto result = opt_max<int>({5, 3, 8, 1, 4}, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 1);
            }

            SUBCASE("variadic with mixed args and comp") {
                auto result = opt_max<int>({10, std::optional{3}, std::nullopt}, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("variadic typed with comp") {
                auto result = opt_max<double>({5, 3, 8}, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 3.0);
            }

            SUBCASE("range with std::greater") {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_max_element(v, std::greater{});
                REQUIRE(result.has_value());
                CHECK(*result == 1);
            }
        }

        TEST_CASE("opt_minmax with custom comparator") {
            SUBCASE("variadic with std::greater reverses order") {
                auto result = opt_minmax<int>({5, 3, 8, 1, 4}, std::greater{});
                REQUIRE(result.has_value());
                CHECK(result->min == 8);
                CHECK(result->max == 1);
            }

            SUBCASE("variadic typed with comp") {
                auto result = opt_minmax<double>({5, 3, 8}, std::greater{});
                REQUIRE(result.has_value());
                CHECK(result->min == 8.0);
                CHECK(result->max == 3.0);
            }

            SUBCASE("range with std::greater") {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_minmax_element(v, std::greater{});
                REQUIRE(result.has_value());
                CHECK(result->min == 8);
                CHECK(result->max == 1);
            }

            SUBCASE("range of optionals with comp") {
                std::vector<std::optional<int>> v{std::nullopt, std::optional{5}, std::optional{2}, std::optional{9}};
                auto result = opt_minmax_element(v, std::greater{});
                REQUIRE(result.has_value());
                CHECK(result->min == 9);
                CHECK(result->max == 2);
            }
        }
    }
}  // namespace dice::template_library
