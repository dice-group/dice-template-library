#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/ipow.hpp>

#include <cstdint>
#include <iterator>
#include <limits>
#include <random>
#include <type_traits>

TEST_SUITE("ipow") {
    using namespace dice::template_library;

    template<typename T>
    void check() {
        if constexpr (std::is_unsigned_v<T>) {
            T expected[] = {25, 65025, 16581375, 16129, 2048383, 1, 0, 1, 1, 0, 2};
            T result[] = {
                ipow(static_cast<T>(5), 2), ipow(static_cast<T>(255), 2), ipow(static_cast<T>(255), 3),
                ipow(static_cast<T>(127), 2), ipow(static_cast<T>(127), 3),
                ipow(static_cast<T>(0), 0), ipow(static_cast<T>(0), 1), ipow(static_cast<T>(1), 0),
                ipow(static_cast<T>(2), 0), ipow(static_cast<T>(0), 2), ipow(static_cast<T>(2), 1),
            };
            for (size_t idx = 0; idx < std::size(expected); ++idx) {
                REQUIRE_EQ(expected[idx], result[idx]);
            }
        } else {
            T expected[] = {25, 65025, 16581375, 16129, 2048383, 1, 0, 1, 1, 0, 2, 1, 1, 1, -1, 1, 0};
            T result[] = {
                ipow(static_cast<T>(5), 2), ipow(static_cast<T>(255), 2), ipow(static_cast<T>(255), 3),
                ipow(static_cast<T>(127), 2), ipow(static_cast<T>(127), 3),
                ipow(static_cast<T>(0), 0), ipow(static_cast<T>(0), 1), ipow(static_cast<T>(1), 0),
                ipow(static_cast<T>(2), 0), ipow(static_cast<T>(0), 2), ipow(static_cast<T>(2), 1),
                ipow(static_cast<T>(-1), 0), ipow(static_cast<T>(1), -1), ipow(static_cast<T>(-1), 2),
                ipow(static_cast<T>(-1), 3), ipow(static_cast<T>(1), -2), ipow(static_cast<T>(2), -2),
            };
            for (size_t idx = 0; idx < std::size(expected); ++idx) {
                REQUIRE_EQ(expected[idx], result[idx]);
            }
        }
    }

    enum struct ipow_outcome { representable, overflows, underflows };

    template<typename T>
    struct ref_ipow_result {
        ipow_outcome outcome;
        T value; ///< only meaningful if outcome == representable
    };

    /**
     * Naive reference implementation: computes the magnitude |base|^exp by
     * repeated multiplication in 128-bit arithmetic and applies the sign of the
     * exact result (negative iff base < 0 and exp is odd) afterwards. The sign
     * decides between over- and underflow; it must come from the *final* result,
     * not from the first out-of-range intermediate, because with a negative base
     * the intermediates alternate sign. The magnitude never shrinks, so bailing
     * out as soon as it exceeds T's range is sound (and keeps the 128-bit
     * arithmetic itself from overflowing).
     * Mirrors ipow's truncation-towards-zero semantics for negative exponents.
     */
    template<typename T>
    ref_ipow_result<T> ref_ipow(T base, int64_t exp) {
        if (exp < 0) {
            if (base == 1) {
                return {ipow_outcome::representable, static_cast<T>(1)};
            }
            if constexpr (std::is_signed_v<T>) {
                if (base == -1) {
                    return {ipow_outcome::representable, static_cast<T>((exp % 2 != 0) ? -1 : 1)};
                }
            }
            return {ipow_outcome::representable, static_cast<T>(0)};
        }

        bool negative = false;
        if constexpr (std::is_signed_v<T>) {
            negative = base < 0 && (exp % 2 != 0);
        }

        auto const wide_base = static_cast<__int128>(base);
        unsigned __int128 const abs_base = static_cast<unsigned __int128>(wide_base < 0 ? -wide_base : wide_base);
        unsigned __int128 const bound = negative
                                            ? static_cast<unsigned __int128>(-static_cast<__int128>(std::numeric_limits<
                                                T>::min()))
                                            : static_cast<unsigned __int128>(std::numeric_limits<T>::max());

        unsigned __int128 magnitude = 1;
        for (int64_t i = 0; i < exp; ++i) {
            magnitude *= abs_base;
            if (magnitude > bound) {
                return {negative ? ipow_outcome::underflows : ipow_outcome::overflows, T{}};
            }
        }

        if (negative) {
            return {ipow_outcome::representable, static_cast<T>(-static_cast<__int128>(magnitude))};
        }
        return {ipow_outcome::representable, static_cast<T>(magnitude)};
    }

    /**
     * Samples the bit length uniformly so that small (non-overflowing) and large
     * (overflowing) bases are both well represented; occasionally returns a
     * boundary value instead.
     */
    template<typename T>
    T random_base(std::mt19937_64 &rng) {
        if (std::uniform_int_distribution<int>{0, 15}(rng) == 0) {
            constexpr T specials[] = {
                0, 1, 2, static_cast<T>(-1), static_cast<T>(-2),
                std::numeric_limits<T>::min(), std::numeric_limits<T>::max()
            };
            return specials[std::uniform_int_distribution<size_t>{0, std::size(specials) - 1}(rng)];
        }

        int const bits = std::uniform_int_distribution<int>{0, std::numeric_limits<T>::digits}(rng);
        uint64_t const magnitude = bits == 0 ? 0 : (rng() & (~uint64_t{0} >> (64 - bits)));

        if constexpr (std::is_signed_v<T>) {
            if (std::uniform_int_distribution<int>{0, 1}(rng) == 1) {
                return static_cast<T>(-static_cast<T>(magnitude));
            }
        }
        return static_cast<T>(magnitude);
    }

    template<typename T>
    void check_random(uint64_t seed, size_t rounds) {
        std::mt19937_64 rng{seed};
        std::uniform_int_distribution<int> exp_dist{-8, 130};

        for (size_t round = 0; round < rounds; ++round) {
            T const base = random_base<T>(rng);
            int const exp = exp_dist(rng);

            CAPTURE(+base);
            CAPTURE(exp);

            auto const [outcome, value] = ref_ipow(base, static_cast<int64_t>(exp));
            switch (outcome) {
                case ipow_outcome::representable: {
                    REQUIRE_EQ(ipow(base, exp), value);
                    if (exp >= 0) {
                        // same expectation with an unsigned exponent type
                        REQUIRE_EQ(ipow(base, static_cast<unsigned>(exp)), value);
                    }
                    break;
                }
                case ipow_outcome::overflows: {
                    REQUIRE_THROWS_AS(ipow(base, exp), std::overflow_error);
                    // with negative exponent: the result is either 0, 1 or -1 (no overflow possible)
                    REQUIRE_THROWS_AS(ipow(base, static_cast<unsigned>(exp)), std::overflow_error);
                    break;
                }
                case ipow_outcome::underflows: {
                    REQUIRE_THROWS_AS(ipow(base, exp), std::underflow_error);
                    // with negative exponent: the result is either 0, 1 or -1 (no underflow possible)
                    REQUIRE_THROWS_AS(ipow(base, static_cast<unsigned>(exp)), std::underflow_error);
                    break;
                }
            }
        }
    }

    TEST_CASE("randomized against naive reference") {
        // fresh seed each run; on failure it is part of the report, hardcode it here to reproduce
        uint64_t const seed = 42;

        constexpr size_t rounds = 10'000;
        check_random<uint8_t>(seed, rounds);
        check_random<int8_t>(seed, rounds);
        check_random<uint16_t>(seed, rounds);
        check_random<int16_t>(seed, rounds);
        check_random<uint32_t>(seed, rounds);
        check_random<int32_t>(seed, rounds);
        check_random<uint64_t>(seed, rounds);
        check_random<int64_t>(seed, rounds);
        check_random<size_t>(seed, rounds);
    }

    TEST_CASE("type case") {
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
        CHECK_THROWS_AS(ipow(-99999, 2), std::overflow_error);
        CHECK_THROWS_AS(ipow(-9999, 3), std::underflow_error);
        CHECK_THROWS_AS(ipow(-100, 9), std::underflow_error);
        CHECK_THROWS_AS(ipow(-1000, 4), std::overflow_error);
        CHECK_THROWS_AS((void) ipow(std::int64_t{-2000000}, 5), std::underflow_error);
        //unsigned
        size_t const base = 9999999999;
        CHECK_THROWS_AS(ipow(base, 2), std::overflow_error);
    }
}
