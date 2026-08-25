#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/try_traits.hpp>

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dtl = dice::template_library;

TEST_SUITE("try_result concept") {
    static_assert(dtl::detail_try_traits::generalized_convertible_to<int, dtl::control_flow<int, double>>);
    static_assert(dtl::detail_try_traits::generalized_convertible_to<void, dtl::control_flow<int, void>>);

    static_assert(dtl::try_result<std::optional<int>>);
    static_assert(dtl::try_result<std::optional<std::string>>);
    static_assert(dtl::try_result<std::optional<std::unique_ptr<int>>>);
    static_assert(dtl::try_result<dtl::control_flow<int, int>>);
    static_assert(dtl::try_result<dtl::control_flow<std::string, std::unique_ptr<int>>>);

#if __cpp_lib_expected >= 202202L
    static_assert(dtl::try_result<std::expected<int, int>>);
    static_assert(dtl::try_result<std::expected<std::string, std::unique_ptr<int>>>);
#endif // __cpp_lib_expected >= 202202L

    // types without a try_traits specialization are not try_results
    static_assert(!dtl::try_result<int>);
    static_assert(!dtl::try_result<std::vector<int>>);
    static_assert(!dtl::try_result<std::variant<int, double>>);
    static_assert(!dtl::try_result<dtl::cfbreak<int>>);
    static_assert(!dtl::try_result<dtl::cfcontinue<int>>);

    // a control_flow with a void arm has either no output_type or no representable residual_type,
    // so it cannot be a try_result (and must not be a hard error to ask)
    static_assert(dtl::try_result<dtl::control_flow<int, void>>);
    static_assert(dtl::try_result<dtl::control_flow<void, int>>);

    TEST_CASE("rebind_output") {
        static_assert(std::same_as<dtl::try_traits<std::optional<int>>::rebind_output<std::string>, std::optional<std::string>>);
        static_assert(std::same_as<dtl::try_traits<dtl::control_flow<int, double>>::rebind_output<std::string>, dtl::control_flow<int, std::string>>);
#if __cpp_lib_expected >= 202202L
        static_assert(std::same_as<dtl::try_traits<std::expected<int, std::string>>::rebind_output<double>, std::expected<double, std::string>>);
#endif // __cpp_lib_expected >= 202202L
    }

    TEST_CASE("residuals stay usable after rebinding the output") {
        static_assert(std::convertible_to<dtl::try_traits<std::optional<int>>::residual_type, std::optional<std::string>>);
        static_assert(std::convertible_to<dtl::try_traits<dtl::control_flow<int, double>>::residual_type, dtl::control_flow<int, std::string>>);
#if __cpp_lib_expected >= 202202L
        static_assert(std::convertible_to<dtl::try_traits<std::expected<int, std::string>>::residual_type, std::expected<double, std::string>>);
#endif // __cpp_lib_expected >= 202202L
    }
}

TEST_SUITE("try_traits") {
    TEST_CASE("optional") {
        using opt = std::optional<int>;
        using traits = dtl::try_traits<opt>;

        SUBCASE("get_output") {
            opt const self{42};
            REQUIRE(traits::has_output(self));
            CHECK_EQ(traits::get_output(self), 42);
        }

        SUBCASE("get_residual") {
            opt const self{std::nullopt};
            REQUIRE_FALSE(traits::has_output(self));

            // the residual is usable for any rebound output type, not just for optional<int>
            std::optional<std::string> const propagated = traits::get_residual(self);
            CHECK_EQ(propagated, std::nullopt);
        }

        SUBCASE("from_output") {
            opt x = 42;
            CHECK_EQ(x.value(), 42);
        }
    }

    TEST_CASE("control_flow") {
        using cf = dtl::control_flow<std::string, int>;
        using traits = dtl::try_traits<cf>;

        SUBCASE("get_output") {
            cf const self = dtl::cfcontinue{42};
            REQUIRE(traits::has_output(self));
            CHECK_EQ(traits::get_output(self), 42);
        }

        SUBCASE("get_residual") {
            cf const self = dtl::cfbreak{std::string{"stop"}};
            REQUIRE_FALSE(traits::has_output(self));
            CHECK_EQ(traits::get_residual(self).value, "stop");
        }

        SUBCASE("from_output") {
            cf cf = 42;
            REQUIRE(cf.is_continue());
            CHECK_EQ(cf.get_continue(), 42);
        }
    }

#if __cpp_lib_expected >= 202202L
    TEST_CASE("expected") {
        using exp = std::expected<int, std::string>;
        using traits = dtl::try_traits<exp>;

        SUBCASE("get_output") {
            exp const self{42};
            REQUIRE(traits::has_output(self));
            CHECK_EQ(traits::get_output(self), 42);
        }

        SUBCASE("get_residual") {
            exp const self{std::unexpect, "boom"};
            REQUIRE_FALSE(traits::has_output(self));
            CHECK_EQ(traits::get_residual(self).error(), "boom");
        }

        SUBCASE("from_output") {
            exp exp = 42;
            REQUIRE(exp.has_value());
            CHECK_EQ(*exp, 42);
        }
    }
#endif // __cpp_lib_expected >= 202202L
}

namespace {
    template<typename Out, typename Ignored>
    std::optional<Out> two_type_params(bool ok) {
        if (!ok) {
            return std::nullopt;
        }

        return Out{1};
    }

    int calls = 0;

    std::optional<int> counted_success() {
        calls += 1;
        return 42;
    }

    /**
     * counts how often it is copied, so that the tests can tell
     * whether DICE_TRY moved or copied the expression it was given
     */
    struct counter {
        static inline int copies = 0;
        static inline int moves = 0;

        int value;

        explicit counter(int value) noexcept : value{value} {
        }

        counter(counter const &other) : value{other.value} {
            copies += 1;
        }

        counter(counter &&other) noexcept : value{other.value} {
            moves += 1;
        }

        static void reset() noexcept {
            copies = 0;
            moves = 0;
        }
    };

    std::optional<counter> make_counted(int value) {
        return counter{value};
    }

    // free function (not a lambda) to make sure DICE_TRY returns from plain functions as well
    std::optional<std::string> free_function(bool ok) {
        auto const x = DICE_TRY(two_type_params<int, double>(ok));
        return std::to_string(x + 1);
    }
} // namespace

TEST_SUITE("DICE_TRY") {
    TEST_CASE("sanity check") {
        auto const fail = [] -> std::optional<int> {
            return std::nullopt;
        };

        auto const success = [] -> std::optional<int> {
            return 42;
        };

        SUBCASE("1") {
            auto const f = [&] -> std::optional<int> {
                auto const x = DICE_TRY(fail());
                return x + 1;
            };

            CHECK_EQ(f(), std::nullopt);
        }

        SUBCASE("2") {
            auto const f = [&] -> std::optional<int> {
                auto const x = DICE_TRY(success());
                return x + 1;
            };

            CHECK_EQ(f(), 43);
        }

        SUBCASE("3") {
            auto const f = [&] -> std::optional<int> {
                return DICE_TRY(success()) + DICE_TRY(fail());
            };

            CHECK_EQ(f(), std::nullopt);
        }
    }

    TEST_CASE("control_flow") {
        auto const brk = [] -> dtl::control_flow<std::string, int> {
            return dtl::cfbreak{std::string{"stop"}};
        };

        auto const cont = [] -> dtl::control_flow<std::string, int> {
            return dtl::cfcontinue{42};
        };

        SUBCASE("break") {
            auto const f = [&] -> dtl::control_flow<std::string, int> {
                auto const x = DICE_TRY(brk());
                return dtl::cfcontinue{x + 1};
            };

            auto const res = f();
            REQUIRE(res.is_break());
            CHECK_EQ(res.get_break(), "stop");
        }

        SUBCASE("continue") {
            auto const f = [&] -> dtl::control_flow<std::string, int> {
                auto const x = DICE_TRY(cont());
                return dtl::cfcontinue{x + 1};
            };

            auto const res = f();
            REQUIRE(res.is_continue());
            CHECK_EQ(res.get_continue(), 43);
        }
    }

#if __cpp_lib_expected >= 202202L
    TEST_CASE("expected") {
        auto const fail = [] -> std::expected<int, std::string> {
            return std::unexpected{"boom"};
        };

        auto const success = [] -> std::expected<int, std::string> {
            return 42;
        };

        SUBCASE("error") {
            auto const f = [&] -> std::expected<int, std::string> {
                auto const x = DICE_TRY(fail());
                return x + 1;
            };

            auto const res = f();
            REQUIRE_FALSE(res.has_value());
            CHECK_EQ(res.error(), "boom");
        }

        SUBCASE("value") {
            auto const f = [&] -> std::expected<int, std::string> {
                auto const x = DICE_TRY(success());
                return x + 1;
            };

            auto const res = f();
            REQUIRE(res.has_value());
            CHECK_EQ(*res, 43);
        }
    }
#endif // __cpp_lib_expected >= 202202L

    TEST_CASE("residual propagates into a differently parameterized try_result") {
        SUBCASE("optional") {
            auto const fail = [] -> std::optional<int> {
                return std::nullopt;
            };

            auto const f = [&] -> std::optional<std::string> {
                auto const x = DICE_TRY(fail());
                return std::to_string(x);
            };

            CHECK_EQ(f(), std::nullopt);
        }

        SUBCASE("control_flow") {
            auto const brk = [] -> dtl::control_flow<std::string, int> {
                return dtl::cfbreak{std::string{"stop"}};
            };

            auto const f = [&] -> dtl::control_flow<std::string, double> {
                auto const x = DICE_TRY(brk());
                return dtl::cfcontinue{static_cast<double>(x)};
            };

            auto const res = f();
            REQUIRE(res.is_break());
            CHECK_EQ(res.get_break(), "stop");
        }

#if __cpp_lib_expected >= 202202L
        SUBCASE("expected") {
            auto const fail = [] -> std::expected<int, std::string> {
                return std::unexpected{"boom"};
            };

            auto const f = [&] -> std::expected<std::string, std::string> {
                auto const x = DICE_TRY(fail());
                return std::to_string(x);
            };

            auto const res = f();
            REQUIRE_FALSE(res.has_value());
            CHECK_EQ(res.error(), "boom");
        }
#endif // __cpp_lib_expected >= 202202L
    }

    TEST_CASE("move only output") {
        auto const fail = [] -> std::optional<std::unique_ptr<int>> {
            return std::nullopt;
        };

        auto const success = [] -> std::optional<std::unique_ptr<int>> {
            return std::make_unique<int>(42);
        };

        SUBCASE("break") {
            auto const f = [&] -> std::optional<int> {
                auto const ptr = DICE_TRY(fail());
                return *ptr;
            };

            CHECK_EQ(f(), std::nullopt);
        }

        SUBCASE("continue") {
            auto const f = [&] -> std::optional<int> {
                auto const ptr = DICE_TRY(success());
                return *ptr;
            };

            CHECK_EQ(f(), 42);
        }
    }

#if __cpp_lib_expected >= 202202L
    TEST_CASE("move only residual") {
        auto const fail = [] -> std::expected<int, std::unique_ptr<int>> {
            return std::unexpected{std::make_unique<int>(42)};
        };

        auto const f = [&] -> std::expected<std::string, std::unique_ptr<int>> {
            auto const x = DICE_TRY(fail());
            return std::to_string(x);
        };

        auto const res = f();
        REQUIRE_FALSE(res.has_value());
        REQUIRE_NE(res.error(), nullptr);
        CHECK_EQ(*res.error(), 42);
    }
#endif // __cpp_lib_expected >= 202202L

    TEST_CASE("the expression is evaluated exactly once") {
        calls = 0;

        auto const f = [] -> std::optional<int> {
            return DICE_TRY(counted_success());
        };

        CHECK_EQ(f(), 42);
        CHECK_EQ(calls, 1);
    }

    TEST_CASE("the expression may contain commas") {
        CHECK_EQ(free_function(true), "2");
        CHECK_EQ(free_function(false), std::nullopt);

        auto const f = [] -> std::optional<int> {
            return DICE_TRY(std::optional<std::pair<int, int>>{std::pair{1, 2}}).first;
        };

        CHECK_EQ(f(), 1);
    }

    TEST_CASE("nested") {
        auto const success = [] -> std::optional<int> {
            return 42;
        };

        auto const fail = [] -> std::optional<int> {
            return std::nullopt;
        };

        auto const twice = [](int x) -> std::optional<int> {
            return x * 2;
        };

        SUBCASE("continue") {
            auto const f = [&] -> std::optional<int> {
                return DICE_TRY(twice(DICE_TRY(success())));
            };

            CHECK_EQ(f(), 84);
        }

        SUBCASE("break") {
            auto const f = [&] -> std::optional<int> {
                return DICE_TRY(twice(DICE_TRY(fail())));
            };

            CHECK_EQ(f(), std::nullopt);
        }
    }

    TEST_CASE("breaks out of loops") {
        auto const half = [](int x) -> std::optional<int> {
            if (x % 2 != 0) {
                return std::nullopt;
            }

            return x / 2;
        };

        auto const f = [&](std::vector<int> const &xs) -> std::optional<int> {
            int sum = 0;
            for (int const x : xs) {
                sum += DICE_TRY(half(x));
            }

            return sum;
        };

        CHECK_EQ(f({2, 4, 6}), 6);
        CHECK_EQ(f({2, 3, 6}), std::nullopt);
    }

    TEST_CASE("move only types") {
        auto const f = [] -> std::optional<int> {
            std::optional<std::unique_ptr<int>> opt = std::make_unique<int>(42);

            auto const ptr = DICE_TRY(std::move(opt));
            CHECK(opt.has_value()); // std::optional stays engaged, its content was moved out
            CHECK_EQ(*opt, nullptr);

            return *ptr;
        };

        CHECK_EQ(f(), 42);

        // note: a move only try_result can only be passed as an rvalue,
        // passing one as an lvalue does not compile because it would have to be copied
    }

    TEST_CASE("only rvalue expressions are moved from") {
        SUBCASE("lvalues are left intact") {
            auto const f = [] -> std::optional<std::size_t> {
                std::optional<std::string> opt{"hello"};

                auto const s = DICE_TRY(opt);
                CHECK_EQ(opt, "hello"); // copied, not moved

                return s.size();
            };

            CHECK_EQ(f(), 5);
        }

        SUBCASE("const lvalues are left intact") {
            auto const f = [] -> std::optional<std::size_t> {
                std::optional<std::string> const opt{"hello"};

                auto const s = DICE_TRY(opt);
                CHECK_EQ(opt, "hello");

                return s.size();
            };

            CHECK_EQ(f(), 5);
        }

        SUBCASE("rvalues are moved from") {
            auto const f = [] -> std::optional<std::size_t> {
                std::optional<std::string> opt{"hello"};

                auto const s = DICE_TRY(std::move(opt));
                REQUIRE(opt.has_value());
                CHECK_NE(opt, "hello"); // moved from

                return s.size();
            };

            CHECK_EQ(f(), 5);
        }
    }

    TEST_CASE("expressions are copied only if they have to be") {
        counter::reset();

        SUBCASE("prvalue") {
            auto const f = [] -> std::optional<int> {
                return DICE_TRY(make_counted(42)).value;
            };

            CHECK_EQ(f(), 42);
            CHECK_EQ(counter::copies, 0);
        }

        SUBCASE("lvalue") {
            auto const f = [] -> std::optional<int> {
                auto opt = make_counted(42);
                counter::reset();

                return DICE_TRY(opt).value;
            };

            CHECK_EQ(f(), 42);
            CHECK_EQ(counter::copies, 1);
        }

        SUBCASE("const lvalue") {
            auto const f = [] -> std::optional<int> {
                auto const opt = make_counted(42);
                counter::reset();

                return DICE_TRY(opt).value;
            };

            CHECK_EQ(f(), 42);
            CHECK_EQ(counter::copies, 1);
        }

        SUBCASE("xvalue") {
            auto const f = [] -> std::optional<int> {
                auto opt = make_counted(42);
                counter::reset();

                return DICE_TRY(std::move(opt)).value;
            };

            CHECK_EQ(f(), 42);
            CHECK_EQ(counter::copies, 0);
        }
    }
}