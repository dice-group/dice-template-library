#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/pointer_tag_pair.hpp>

#include <concepts>
#include <cstdint>

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

    TEST_CASE("nullptr with non-zero tag") {
        pair_type x{nullptr, 3};
        REQUIRE_EQ(x.pointer(), nullptr);
        REQUIRE_EQ(x.tag(), 3);
    }

    TEST_CASE("default construction") {
        pair_type x;
        REQUIRE_EQ(x.pointer(), nullptr);
        REQUIRE_EQ(x.tag(), 0);
    }

    TEST_CASE("all low-bit tag values round-trip") {
        for (unsigned tag = 0; tag < (1u << pair_type::bits_requested); ++tag) {
            pair_type x{array, tag};
            REQUIRE_EQ(x.pointer(), array);
            REQUIRE_EQ(x.tag(), tag);

            pair_type y{&array[1], tag};
            REQUIRE_EQ(y.pointer(), &array[1]);
            REQUIRE_EQ(y.tag(), tag);
        }
    }

    TEST_CASE("tagged_pointer / from_tagged round-trip") {
        pair_type x{array, 3};
        auto const raw = x.tagged_pointer();

        auto const y = pair_type::from_tagged(raw);
        REQUIRE_EQ(y.pointer(), array);
        REQUIRE_EQ(y.tag(), 3);
        REQUIRE_EQ(x, y);
    }

    TEST_CASE("swap") {
        pair_type a{array, 1};
        pair_type b{&array[1], 2};

        swap(a, b);

        REQUIRE_EQ(a.pointer(), &array[1]);
        REQUIRE_EQ(a.tag(), 2);
        REQUIRE_EQ(b.pointer(), array);
        REQUIRE_EQ(b.tag(), 1);
    }

    TEST_CASE("comparison") {
        pair_type const a{array, 1};
        pair_type const b{array, 1};
        pair_type const c{array, 2};

        REQUIRE_EQ(a, b);
        REQUIRE_NE(a, c);
        // same pointer, tag participates in the ordering
        REQUIRE_LT(a, c);
    }

    TEST_CASE("structured bindings and get<>") {
        pair_type const x{array, 3};

        REQUIRE_EQ(get<0>(x), array);
        REQUIRE_EQ(get<1>(x), 3);

        auto const [p, t] = x;
        REQUIRE_EQ(p, array);
        REQUIRE_EQ(t, 3);

        static_assert(std::tuple_size_v<pair_type> == 2);
        static_assert(std::is_same_v<std::tuple_element_t<0, pair_type>, s *>);
        static_assert(std::is_same_v<std::tuple_element_t<1, pair_type>, unsigned>);
    }

    TEST_CASE("enum tag") {
        enum class color : unsigned {
            red = 0,
            green = 1,
            blue = 3,
        };

        using enum_pair = pointer_tag_pair<s, color>;
        enum_pair const x{array, color::blue};
        REQUIRE_EQ(x.pointer(), array);
        REQUIRE_EQ(x.tag(), color::blue);
    }

    TEST_CASE("from_overaligned uses promised alignment for extra low bits") {
        // `s` only guarantees alignof(4) == 2 low bits, but this storage is 16-aligned,
        // so we may borrow 4 low bits.
        alignas(16) static s overaligned[2];

        using over_pair = pointer_tag_pair<s, unsigned, 4>;
        static_assert(over_pair::bits_requested == 4);

        auto const x = over_pair::from_overaligned<16>(overaligned, 15);
        REQUIRE_EQ(x.pointer(), overaligned);
        REQUIRE_EQ(x.tag(), 15);
    }

    // ---- high-bit tagging (the aggressive-pointer-tagging feature) ----

    TEST_SUITE("high bits") {
        struct alignas(8) node {
            int value;
        };

        static node nodes[2];

        TEST_CASE("bits_requested combines low and high") {
            using pp = pointer_tag_pair<node, unsigned, 3, 4>;
            static_assert(pp::bits_requested == 7);
            static_assert(pointer_tag_pair<node, unsigned, 0, 4>::bits_requested == 4);
            static_assert(pointer_tag_pair<node, unsigned, 3, 0>::bits_requested == 3);
        }

        TEST_CASE("full tag range round-trips through low+high bits") {
            using pp = pointer_tag_pair<node, unsigned, 3, 4>; // 7 bits total -> 0..127

            for (unsigned tag = 0; tag < (1u << pp::bits_requested); ++tag) {
                pp const x{nodes, tag};
                REQUIRE_EQ(x.pointer(), nodes);
                REQUIRE_EQ(x.tag(), tag);

                pp const y{&nodes[1], tag};
                REQUIRE_EQ(y.pointer(), &nodes[1]);
                REQUIRE_EQ(y.tag(), tag);
            }
        }

        TEST_CASE("high-only tags round-trip") {
            using pp = pointer_tag_pair<node, unsigned, 0, 4>; // 4 high bits, no low bits

            for (unsigned tag = 0; tag < 16; ++tag) {
                pp const x{nodes, tag};
                REQUIRE_EQ(x.pointer(), nodes);
                REQUIRE_EQ(x.tag(), tag);
            }
        }

        TEST_CASE("high bits do not leak into the low tag part") {
            // A tag whose only set bit lives in the high region must decode to exactly
            // that value and leave the low part zero. This is the case the precedence
            // bug got wrong (high tag bits silently dropped).
            using pp = pointer_tag_pair<node, unsigned, 3, 4>;

            constexpr unsigned first_high_bit = 1u << 3; // 8
            pp const x{nodes, first_high_bit};
            REQUIRE_EQ(x.pointer(), nodes);
            REQUIRE_EQ(x.tag(), first_high_bit);

            // low and high parts are independent
            constexpr unsigned mixed = (1u << 3) | 0b101; // one high bit + low bits
            pp const y{nodes, mixed};
            REQUIRE_EQ(y.pointer(), nodes);
            REQUIRE_EQ(y.tag(), mixed);
        }

        TEST_CASE("pointer stays clean regardless of tag") {
            using pp = pointer_tag_pair<node, unsigned, 3, 4>;

            pp const zero{nodes, 0};
            pp const full{nodes, 127};

            // The recovered pointer is identical no matter what tag bits are set.
            REQUIRE_EQ(zero.pointer(), full.pointer());
            REQUIRE_EQ(full.pointer(), nodes);
            REQUIRE_EQ(full.pointer()->value, nodes->value);
        }

        TEST_CASE("high-bit tagged_pointer / from_tagged round-trip") {
            using pp = pointer_tag_pair<node, unsigned, 3, 4>;

            pp const x{&nodes[1], 100};
            auto const y = pp::from_tagged(x.tagged_pointer());

            REQUIRE_EQ(y.pointer(), &nodes[1]);
            REQUIRE_EQ(y.tag(), 100);
            REQUIRE_EQ(x, y);
        }

        TEST_CASE("high-bit nullptr round-trip") {
            using pp = pointer_tag_pair<node, unsigned, 3, 4>;

            pp const x{nullptr, 127};
            REQUIRE_EQ(x.pointer(), nullptr);
            REQUIRE_EQ(x.tag(), 127);
        }

        TEST_CASE("high-bit enum tag") {
            enum class flags : std::uint32_t {
                none = 0,
                high_only = 1u << 3,
                all = 127,
            };

            using pp = pointer_tag_pair<node, flags, 3, 4>;
            pp const x{nodes, flags::high_only};
            REQUIRE_EQ(x.pointer(), nodes);
            REQUIRE_EQ(x.tag(), flags::high_only);

            pp const y{nodes, flags::all};
            REQUIRE_EQ(y.tag(), flags::all);
        }

        TEST_CASE("high-bit structured bindings") {
            using pp = pointer_tag_pair<node, unsigned, 3, 4>;

            auto const [p, t] = pp{nodes, 42};
            REQUIRE_EQ(p, nodes);
            REQUIRE_EQ(t, 42);
        }

        TEST_CASE("high-bit swap and comparison") {
            using pp = pointer_tag_pair<node, unsigned, 3, 4>;

            pp a{nodes, 100};
            pp b{&nodes[1], 20};

            REQUIRE_NE(a, b);

            swap(a, b);
            REQUIRE_EQ(a.pointer(), &nodes[1]);
            REQUIRE_EQ(a.tag(), 20);
            REQUIRE_EQ(b.pointer(), nodes);
            REQUIRE_EQ(b.tag(), 100);
        }
    }

    // specific-hardware factories: exceeding the conservative high-bit cap
    //
    // The normal constructor and from_overaligned cap the high bits at
    // conservative_hardware_taggable_high_bits_available (4) -- the number safe on *every*
    // supported architecture. from_specific_hardware / from_overaligned_specific_hardware drop
    // that cap so callers who know their target hardware can tag more MSBs.
#if defined(__x86_64__) || defined(_M_X64)
    TEST_SUITE("specific hardware (x86-64)") {
        struct alignas(8) node { // alignof(8) -> 3 natural low bits
            int value;
        };

        static node nodes[2];
        alignas(16) static node aligned_nodes[2]; // 4 low bits, but only when the alignment is promised

        static constexpr unsigned x86_64_high_bits = 7; // README: at most 7 high bits are safe on x86-64

        TEST_CASE("the conservative cap rejects >4 high bits without the escape hatch") {
            using pp = pointer_tag_pair<node, unsigned, 3, x86_64_high_bits>;
            static_assert(pp::bits_requested == 10);

            // 7 high bits exceeds the conservative cap (4), so the normal constructor
            // is SFINAE'd away -- you must opt in via from_specific_hardware.
            static_assert(!std::constructible_from<pp, node *, unsigned>,
                          "normal constructor must reject more high bits than the conservative cap");

            pp const x = pp::from_specific_hardware(nodes, 1023);
            REQUIRE_EQ(x.pointer(), nodes);
            REQUIRE_EQ(x.tag(), 1023);
        }

        TEST_CASE("from_specific_hardware: full 3 low + 7 high bit range round-trips") {
            using pp = pointer_tag_pair<node, unsigned, 3, x86_64_high_bits>; // 10 bits -> 0..1023

            for (unsigned tag = 0; tag < (1u << pp::bits_requested); ++tag) {
                pp const x = pp::from_specific_hardware(nodes, tag);
                REQUIRE_EQ(x.pointer(), nodes);
                REQUIRE_EQ(x.tag(), tag);

                pp const y = pp::from_specific_hardware(&nodes[1], tag);
                REQUIRE_EQ(y.pointer(), &nodes[1]);
                REQUIRE_EQ(y.tag(), tag);
            }
        }

        TEST_CASE("from_specific_hardware: all 7 bits packed into the MSBs, none in the LSBs") {
            using pp = pointer_tag_pair<node, unsigned, 0, x86_64_high_bits>; // 0..127, entirely in high bits

            for (unsigned tag = 0; tag < (1u << pp::bits_requested); ++tag) {
                pp const x = pp::from_specific_hardware(nodes, tag);
                REQUIRE_EQ(x.pointer(), nodes);
                REQUIRE_EQ(x.tag(), tag);
            }
        }

        TEST_CASE("from_specific_hardware round-trips via tagged_pointer / from_tagged") {
            using pp = pointer_tag_pair<node, unsigned, 3, x86_64_high_bits>;

            pp const x = pp::from_specific_hardware(&nodes[1], 1000);
            auto const y = pp::from_tagged(x.tagged_pointer());

            REQUIRE_EQ(y.pointer(), &nodes[1]);
            REQUIRE_EQ(y.tag(), 1000);
            REQUIRE_EQ(x, y);
        }

        TEST_CASE("from_overaligned_specific_hardware combines a promised alignment with >4 high bits") {
            // 4 low bits requires a promised alignment of 16 (node only guarantees 8 -> 3 bits) AND
            // 7 high bits requires the specific-hardware escape hatch. 11 bits total -> 0..2047.
            using pp = pointer_tag_pair<node, unsigned, 4, x86_64_high_bits>;
            static_assert(pp::bits_requested == 11);

            // The normal constructor rejects this on both counts (extra low bit and extra high bits).
            static_assert(!std::constructible_from<pp, node *, unsigned>);

            for (unsigned tag = 0; tag < (1u << pp::bits_requested); ++tag) {
                // Only the 16-aligned element 0 satisfies the promised alignment; &aligned_nodes[1]
                // is merely 4-byte aligned, so we deliberately do not tag it here.
                pp const x = pp::from_overaligned_specific_hardware<16>(aligned_nodes, tag);
                REQUIRE_EQ(x.pointer(), aligned_nodes);
                REQUIRE_EQ(x.tag(), tag);
            }
        }

        TEST_CASE("from_overaligned_specific_hardware: pointer stays clean at the tag extremes") {
            using pp = pointer_tag_pair<node, unsigned, 4, x86_64_high_bits>;

            pp const zero = pp::from_overaligned_specific_hardware<16>(aligned_nodes, 0);
            pp const full = pp::from_overaligned_specific_hardware<16>(aligned_nodes, 2047);

            REQUIRE_EQ(zero.pointer(), full.pointer());
            REQUIRE_EQ(full.pointer(), aligned_nodes);
            REQUIRE_EQ(full.pointer()->value, aligned_nodes->value);
            REQUIRE_EQ(zero.tag(), 0);
            REQUIRE_EQ(full.tag(), 2047);
        }
    }
#endif
}
