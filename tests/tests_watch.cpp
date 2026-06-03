#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/watch.hpp>

TEST_SUITE("watch") {
    TEST_CASE("sanity check") {
        dice::template_library::watch<int> w;

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
