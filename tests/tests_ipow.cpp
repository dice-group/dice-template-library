#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/ipow.hpp>

#include <cstddef>
#include <cstdint>

TEST_SUITE("ipow") {
    using namespace dice::template_library::math;

    template<typename T>
    void check() {
        if constexpr (std::is_unsigned_v<T>) {
            T expected[] = {25, 65025, 16581375, 16129, 2048383, 1, 0, 1, 1, 0, 2};
            T result[] = {
                ipow(5, 2), ipow(255, 2), ipow(255, 3), ipow(127, 2), ipow(127, 3),
                ipow(0, 0), ipow(0, 1), ipow(1, 0), ipow(2, 0), ipow(0, 2), ipow(2, 1),
            };
            for (size_t idx = 0; idx < std::size(expected); ++idx) {
                REQUIRE_EQ(expected[idx], result[idx]);
            }
        } else {
            T expected[] = {25, 65025, 16581375, 16129, 2048383, 1, 0, 1, 1, 0, 2, 1, 1, 1, -1, 1, 0};
            T result[] = {
                ipow(5, 2), ipow(255, 2), ipow(255, 3), ipow(127, 2), ipow(127, 3),
                ipow(0, 0), ipow(0, 1), ipow(1, 0), ipow(2, 0), ipow(0, 2), ipow(2, 1),
                ipow(-1, 0), ipow(1, -1), ipow(-1, 2), ipow(-1, 3), ipow(1, -2), ipow(2, -2),
            };
            for (size_t idx = 0; idx < std::size(expected); ++idx) {
                REQUIRE_EQ(expected[idx], result[idx]);
            }
        }
    }

    TEST_CASE("type case" ) {
        check<size_t>();
        check<uint64_t>();
        check<uint32_t>();
        check<int>();
        check<int32_t>();
        check<int64_t>();
    }

    TEST_CASE("numeric limits") {
        //signed
        CHECK_THROWS_AS(ipow(99999, 2), std::overflow_error);
        //CHECK_THROWS_AS(ipow(10, std::numeric_limits<int>::min()));
        CHECK_THROWS_AS(ipow(-99999, 2), std::overflow_error);
        CHECK_THROWS_AS(ipow(-9999, 3), std::underflow_error);
        int32_t const base_int = -100;
        CHECK_THROWS_AS(ipow(base_int, 3), std::underflow_error);
        //unsigned
        size_t const base = 9999999999;
        CHECK_THROWS_AS(ipow(base, 2), std::overflow_error);
    }
}
