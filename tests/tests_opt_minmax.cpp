#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/opt_minmax.hpp>

#include <doctest/doctest.h>

#include <vector>

namespace dice::template_library
{
    TEST_SUITE("opt_minmax")
    {
        TEST_CASE("opt_min")
        {
            SUBCASE("0 args returns nullopt")
            {
                auto result = opt_min();
                CHECK(std::same_as<decltype(result), std::nullopt_t>);
            }

            SUBCASE("0 args typed returns empty optional")
            {
                auto result = opt_min<size_t>();
                CHECK(std::same_as<decltype(result), std::optional<size_t>>);
                CHECK(!result.has_value());
            }

            SUBCASE("1 plain arg")
            {
                auto result = opt_min(42);
                REQUIRE(result.has_value());
                CHECK(*result == 42);
            }

            SUBCASE("1 optional arg with value")
            {
                auto result = opt_min(std::optional{7});
                REQUIRE(result.has_value());
                CHECK(*result == 7);
            }

            SUBCASE("1 optional arg nullopt")
            {
                auto result = opt_min(std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("mixed plain and optional")
            {
                auto result = opt_min(10, std::optional{3}, 5);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("all nullopt")
            {
                auto result = opt_min(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("basic comparison")
            {
                auto result = opt_min(5, 3, 8, 1, 4);
                REQUIRE(result.has_value());
                CHECK(*result == 1);
            }

            SUBCASE("negative numbers")
            {
                auto result = opt_min(-5, 3, -8, 1);
                REQUIRE(result.has_value());
                CHECK(*result == -8);
            }

            SUBCASE("all same values")
            {
                auto result = opt_min(4, 4, 4);
                REQUIRE(result.has_value());
                CHECK(*result == 4);
            }

            SUBCASE("2-mix: plain, plain")
            {
                auto result = opt_min(5, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: plain, empty optional")
            {
                auto result = opt_min(5, std::optional<int>{});
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: plain, nullopt")
            {
                auto result = opt_min(5, std::nullopt);
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: empty optional, plain")
            {
                auto result = opt_min(std::optional<int>{}, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: empty optional, empty optional")
            {
                auto result = opt_min(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: empty optional, nullopt")
            {
                auto result = opt_min(std::optional<int>{}, std::nullopt);
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, plain")
            {
                auto result = opt_min(std::nullopt, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: nullopt, empty optional")
            {
                auto result = opt_min(std::nullopt, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, nullopt")
            {
                auto result = opt_min(std::nullopt, std::nullopt);
                CHECK(std::same_as<decltype(result), std::nullopt_t>);
            }
        }

        TEST_CASE("opt_max")
        {
            SUBCASE("0 args returns nullopt")
            {
                auto result = opt_max();
                CHECK(std::same_as<decltype(result), std::nullopt_t>);
            }

            SUBCASE("0 args typed returns empty optional")
            {
                auto result = opt_max<size_t>();
                CHECK(std::same_as<decltype(result), std::optional<size_t>>);
                CHECK(!result.has_value());
            }

            SUBCASE("1 plain arg")
            {
                auto result = opt_max(42);
                REQUIRE(result.has_value());
                CHECK(*result == 42);
            }

            SUBCASE("1 optional arg with value")
            {
                auto result = opt_max(std::optional{7});
                REQUIRE(result.has_value());
                CHECK(*result == 7);
            }

            SUBCASE("1 optional arg nullopt")
            {
                auto result = opt_max(std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("mixed plain and optional")
            {
                auto result = opt_max(10, std::optional{3}, 5);
                REQUIRE(result.has_value());
                CHECK(*result == 10);
            }

            SUBCASE("all nullopt")
            {
                auto result = opt_max(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("basic comparison")
            {
                auto result = opt_max(5, 3, 8, 1, 4);
                REQUIRE(result.has_value());
                CHECK(*result == 8);
            }

            SUBCASE("negative numbers")
            {
                auto result = opt_max(-5, 3, -8, 1);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("all same values")
            {
                auto result = opt_max(4, 4, 4);
                REQUIRE(result.has_value());
                CHECK(*result == 4);
            }

            SUBCASE("2-mix: plain, plain")
            {
                auto result = opt_max(3, 5);
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: plain, empty optional")
            {
                auto result = opt_max(5, std::optional<int>{});
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: plain, nullopt")
            {
                auto result = opt_max(5, std::nullopt);
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }

            SUBCASE("2-mix: empty optional, plain")
            {
                auto result = opt_max(std::optional<int>{}, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: empty optional, empty optional")
            {
                auto result = opt_max(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: empty optional, nullopt")
            {
                auto result = opt_max(std::optional<int>{}, std::nullopt);
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, plain")
            {
                auto result = opt_max(std::nullopt, 3);
                REQUIRE(result.has_value());
                CHECK(*result == 3);
            }

            SUBCASE("2-mix: nullopt, empty optional")
            {
                auto result = opt_max(std::nullopt, std::optional<int>{});
                CHECK(!result.has_value());
            }

            SUBCASE("2-mix: nullopt, nullopt")
            {
                auto result = opt_max(std::nullopt, std::nullopt);
                CHECK(std::same_as<decltype(result), std::nullopt_t>);
            }
        }

        TEST_CASE("opt_minmax")
        {
            SUBCASE("0 args")
            {
                opt_minmax_result<int> result = opt_minmax();
                CHECK(!result.min.has_value());
                CHECK(!result.max.has_value());
            }

            SUBCASE("0 args typed")
            {
                auto result = opt_minmax<size_t>();
                CHECK(std::same_as<decltype(result), opt_minmax_result<size_t>>);
                CHECK(!result.min.has_value());
                CHECK(!result.max.has_value());
            }

            SUBCASE("single value")
            {
                auto result = opt_minmax(42);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 42);
                CHECK(*result.max == 42);
            }

            SUBCASE("multiple values")
            {
                auto result = opt_minmax(5, 3, 8, 1, 4);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 1);
                CHECK(*result.max == 8);
            }

            SUBCASE("all nullopt")
            {
                auto result = opt_minmax(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.min.has_value());
                CHECK(!result.max.has_value());
            }

            SUBCASE("mixed plain and optional")
            {
                auto result = opt_minmax(10, std::optional<int>{}, std::optional{3}, 5);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 3);
                CHECK(*result.max == 10);
            }

            SUBCASE("2-mix: plain, plain")
            {
                auto result = opt_minmax(3, 5);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 3);
                CHECK(*result.max == 5);
            }

            SUBCASE("2-mix: plain, empty optional")
            {
                auto result = opt_minmax(5, std::optional<int>{});
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 5);
                CHECK(*result.max == 5);
            }

            SUBCASE("2-mix: plain, nullopt")
            {
                auto result = opt_minmax(5, std::nullopt);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 5);
                CHECK(*result.max == 5);
            }

            SUBCASE("2-mix: empty optional, plain")
            {
                auto result = opt_minmax(std::optional<int>{}, 3);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 3);
                CHECK(*result.max == 3);
            }

            SUBCASE("2-mix: empty optional, empty optional")
            {
                auto result = opt_minmax(std::optional<int>{}, std::optional<int>{});
                CHECK(!result.min.has_value());
                CHECK(!result.max.has_value());
            }

            SUBCASE("2-mix: empty optional, nullopt")
            {
                auto result = opt_minmax(std::optional<int>{}, std::nullopt);
                CHECK(!result.min.has_value());
                CHECK(!result.max.has_value());
            }

            SUBCASE("2-mix: nullopt, plain")
            {
                auto result = opt_minmax(std::nullopt, 3);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 3);
                CHECK(*result.max == 3);
            }

            SUBCASE("2-mix: nullopt, empty optional")
            {
                auto result = opt_minmax(std::nullopt, std::optional<int>{});
                CHECK(!result.min.has_value());
                CHECK(!result.max.has_value());
            }

            SUBCASE("2-mix: nullopt, nullopt")
            {
                auto result = opt_minmax(std::nullopt, std::nullopt);
                CHECK(std::same_as<decltype(result), empty_opt_minmax_result>);
            }
        }

        TEST_CASE("opt_min_range")
        {
            SUBCASE("empty range")
            {
                std::vector<int> v;
                auto result = opt_min_range(v);
                CHECK(!result.has_value());
            }

            SUBCASE("single element")
            {
                std::vector<int> v{42};
                auto result = opt_min_range(v);
                REQUIRE(result.has_value());
                CHECK(*result == 42);
            }

            SUBCASE("multiple elements")
            {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_min_range(v);
                REQUIRE(result.has_value());
                CHECK(*result == 1);
            }

            SUBCASE("range of optionals")
            {
                std::vector<std::optional<int>> v{std::nullopt, std::optional{5}, std::optional{2}, std::nullopt};
                auto result = opt_min_range(v);
                REQUIRE(result.has_value());
                CHECK(*result == 2);
            }

            SUBCASE("range of all nullopt optionals")
            {
                std::vector<std::optional<int>> v{std::nullopt, std::nullopt};
                auto result = opt_min_range(v);
                CHECK(!result.has_value());
            }
        }

        TEST_CASE("opt_max_range")
        {
            SUBCASE("empty range")
            {
                std::vector<int> v;
                auto result = opt_max_range(v);
                CHECK(!result.has_value());
            }

            SUBCASE("single element")
            {
                std::vector<int> v{42};
                auto result = opt_max_range(v);
                REQUIRE(result.has_value());
                CHECK(*result == 42);
            }

            SUBCASE("multiple elements")
            {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_max_range(v);
                REQUIRE(result.has_value());
                CHECK(*result == 8);
            }

            SUBCASE("range of optionals")
            {
                std::vector<std::optional<int>> v{std::nullopt, std::optional{5}, std::optional{2}, std::nullopt};
                auto result = opt_max_range(v);
                REQUIRE(result.has_value());
                CHECK(*result == 5);
            }
        }

        TEST_CASE("opt_minmax_range")
        {
            SUBCASE("empty range")
            {
                std::vector<int> v;
                auto result = opt_minmax_range(v);
                CHECK(!result.min.has_value());
                CHECK(!result.max.has_value());
            }

            SUBCASE("single element")
            {
                std::vector<int> v{42};
                auto result = opt_minmax_range(v);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 42);
                CHECK(*result.max == 42);
            }

            SUBCASE("multiple elements")
            {
                std::vector<int> v{5, 3, 8, 1, 4};
                auto result = opt_minmax_range(v);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 1);
                CHECK(*result.max == 8);
            }

            SUBCASE("range of optionals")
            {
                std::vector<std::optional<int>> v{
                    std::nullopt, std::optional{5}, std::optional{2}, std::nullopt, std::optional{9}
                };
                auto result = opt_minmax_range(v);
                REQUIRE(result.min.has_value());
                REQUIRE(result.max.has_value());
                CHECK(*result.min == 2);
                CHECK(*result.max == 9);
            }
        }
    }
} // namespace dice::template_library
