#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/pointer_tag_pair.hpp>

TEST_SUITE("pointer_tag_pair") {
    using namespace dice::template_library;

    struct alignas(4) s {
    };

    using pair_type = pointer_tag_pair<s, unsigned>;
    static_assert(pair_type::bits_requested == 2);

    static s array[2];

    TEST_CASE("constructor") {
        auto const [p, t] = pair_type{array, 3};
        REQUIRE(p == array);
        REQUIRE(t == 3);
    }

    TEST_CASE("nullptr conversion") {
        pair_type x = nullptr;
        REQUIRE_EQ(x.pointer(), nullptr);
        REQUIRE_EQ(x.tag(), 0);
    }
}
