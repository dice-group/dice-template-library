#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/control_flow.hpp>
#include <dice/template-library/overloaded.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace dtl = dice::template_library;

namespace {
    template<typename T>
    concept has_is_continue = requires(T cf) { cf.is_continue(); };

    template<typename T>
    concept has_get_continue = requires(T cf) { cf.get_continue(); };

    template<typename T>
    concept has_is_break = requires(T cf) { cf.is_break(); };

    template<typename T>
    concept has_get_break = requires(T cf) { cf.get_break(); };
} // namespace

TEST_SUITE("cfbreak and cfcontinue") {
    TEST_CASE("construction") {
        static_assert(std::same_as<decltype(dtl::cfbreak{42}), dtl::cfbreak<int>>);
        static_assert(std::same_as<decltype(dtl::cfcontinue{42}), dtl::cfcontinue<int>>);

        // cfcontinue defaults to monostate for computations that have nothing to carry
        static_assert(std::same_as<dtl::cfcontinue<>, dtl::cfcontinue<void>>);

        CHECK_EQ(dtl::cfbreak{42}.value, 42);
        CHECK_EQ(dtl::cfcontinue{42}.value, 42);
    }

    TEST_CASE("comparison") {
        CHECK_EQ(dtl::cfbreak{42}, dtl::cfbreak{42});
        CHECK_NE(dtl::cfbreak{42}, dtl::cfbreak{43});
        CHECK_LT(dtl::cfbreak{42}, dtl::cfbreak{43});

        CHECK_EQ(dtl::cfcontinue{42}, dtl::cfcontinue{42});
        CHECK_NE(dtl::cfcontinue{42}, dtl::cfcontinue{43});
        CHECK_LT(dtl::cfcontinue{42}, dtl::cfcontinue{43});

        CHECK_EQ(dtl::cfcontinue<>{}, dtl::cfcontinue<>{});
    }
}

TEST_SUITE("control_flow") {
    using cf_int = dtl::control_flow<std::string, int>;

    static_assert(!std::is_default_constructible_v<cf_int>);

    // control_flow defaults its continue type void
    static_assert(std::same_as<dtl::control_flow<int>, dtl::control_flow<int, void>>);

    TEST_CASE("construction from an arm") {
        SUBCASE("continue") {
            cf_int const cf{dtl::cfcontinue{42}};
            REQUIRE(cf.is_continue());
            REQUIRE_FALSE(cf.is_break());
            CHECK_EQ(cf.get_continue(), 42);
        }

        SUBCASE("break") {
            cf_int const cf{dtl::cfbreak{std::string{"stop"}}};
            REQUIRE(cf.is_break());
            REQUIRE_FALSE(cf.is_continue());
            CHECK_EQ(cf.get_break(), "stop");
        }

        SUBCASE("copy of an arm") {
            dtl::cfcontinue<int> const cont{42};
            cf_int const cf{cont};
            REQUIRE(cf.is_continue());
            CHECK_EQ(cf.get_continue(), 42);
            CHECK_EQ(cont.value, 42); // untouched
        }
    }

    TEST_CASE("in place construction") {
        SUBCASE("continue") {
            cf_int const cf{dtl::in_place_continue, 42};
            REQUIRE(cf.is_continue());
            CHECK_EQ(cf.get_continue(), 42);
        }

        SUBCASE("break") {
            cf_int const cf{dtl::in_place_break, "stop"};
            REQUIRE(cf.is_break());
            CHECK_EQ(cf.get_break(), "stop");
        }
    }

    TEST_CASE("get_* propagates the value category of the control_flow") {
        static_assert(std::same_as<decltype(std::declval<cf_int &>().get_continue()), int &>);
        static_assert(std::same_as<decltype(std::declval<cf_int const &>().get_continue()), int const &>);
        static_assert(std::same_as<decltype(std::declval<cf_int &&>().get_continue()), int &&>);

        static_assert(std::same_as<decltype(std::declval<cf_int &>().get_break()), std::string &>);
        static_assert(std::same_as<decltype(std::declval<cf_int const &>().get_break()), std::string const &>);
        static_assert(std::same_as<decltype(std::declval<cf_int &&>().get_break()), std::string &&>);

        SUBCASE("mutation through the reference") {
            cf_int cf{dtl::cfcontinue{42}};
            cf.get_continue() = 43;
            CHECK_EQ(cf.get_continue(), 43);
        }

        SUBCASE("moving the value out") {
            dtl::control_flow<int, std::unique_ptr<int>> cf{dtl::cfcontinue{std::make_unique<int>(42)}};

            auto const ptr = std::move(cf).get_continue();
            REQUIRE_NE(ptr, nullptr);
            CHECK_EQ(*ptr, 42);
            CHECK_EQ(cf.get_continue(), nullptr); // moved from
        }
    }

    TEST_CASE("visit and match") {
        SUBCASE("visit") {
            cf_int const cf{dtl::cfcontinue{42}};

            auto const res = visit(dtl::overloaded{
                                       [](dtl::cfcontinue<int> const &cont) { return cont.value; },
                                       [](dtl::cfbreak<std::string> const &) { return -1; }},
                                   cf);

            CHECK_EQ(res, 42);
        }

        SUBCASE("match") {
            cf_int const cf{dtl::cfbreak{std::string{"stop"}}};

            auto const res = match(
                    cf,
                    [](dtl::cfcontinue<int> const &cont) { return std::to_string(cont.value); },
                    [](dtl::cfbreak<std::string> const &brk) { return brk.value; });

            CHECK_EQ(res, "stop");
        }
    }

    TEST_CASE("comparison") {
        CHECK_EQ(cf_int{dtl::cfcontinue{42}}, cf_int{dtl::cfcontinue{42}});
        CHECK_NE(cf_int{dtl::cfcontinue{42}}, cf_int{dtl::cfcontinue{43}});
        CHECK_NE(cf_int{dtl::cfcontinue{42}}, cf_int{dtl::cfbreak{std::string{"stop"}}});
        CHECK_EQ(cf_int{dtl::cfbreak{std::string{"stop"}}}, cf_int{dtl::cfbreak{std::string{"stop"}}});
    }

    TEST_CASE("conversion from a single armed control_flow") {
        SUBCASE("break only") {
            dtl::cfbreak<std::string> const residual{dtl::cfbreak{std::string{"stop"}}};

            cf_int const cf = residual;
            REQUIRE(cf.is_break());
            CHECK_EQ(cf.get_break(), "stop");
            CHECK_EQ(residual.value, "stop"); // copied, not moved

            // the whole point of the conversion: a residual works for any continue type
            dtl::control_flow<std::string, double> const other = residual;
            REQUIRE(other.is_break());
            CHECK_EQ(other.get_break(), "stop");
        }

        SUBCASE("break only, moved") {
            dtl::cfbreak<std::unique_ptr<int>> residual{dtl::cfbreak{std::make_unique<int>(42)}};

            dtl::control_flow<std::unique_ptr<int>, std::string> const cf = std::move(residual);
            REQUIRE(cf.is_break());
            REQUIRE_NE(cf.get_break(), nullptr);
            CHECK_EQ(*cf.get_break(), 42);
            CHECK_EQ(residual.value, nullptr); // moved from
        }
    }
}