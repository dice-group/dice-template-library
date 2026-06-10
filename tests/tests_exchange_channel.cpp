#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/exchange_channel.hpp>

TEST_SUITE("exchange_channel") {
    TEST_CASE("sanity check") {
        dice::template_library::exchange_channel<int> w;

        w.push(1);
        CHECK_EQ(w.pop(), 1);
        CHECK_FALSE(w.try_pop().has_value());

        w.push(2);
        w.push(3);
        CHECK_EQ(w.pop(), 3);
        CHECK_FALSE(w.try_pop().has_value());

        w.close();
        CHECK_FALSE(w.pop().has_value());
        CHECK_FALSE(w.try_pop().has_value());
    }
}
