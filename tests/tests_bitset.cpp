#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include <dice/template-library/bitset.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
	// non-integral, multi-word segment type (two 64-bit words per segment) used to exercise the
	// code paths that only trigger for T where alignof(T) < sizeof(T), e.g. segment_slots /
	// slot_handler / get_chunk_raw, alongside the plain-integral segment tests below.
	struct wide_word {
		std::uint64_t lo{};
		std::uint64_t hi{};
	};
} // namespace

TEST_SUITE("bitset") {
	using namespace dice::template_library;

	using dyn64 = bitset<std::uint64_t, dynamic_extent, dynamic_extent>;
	using dyn8 = bitset<std::uint8_t, dynamic_extent, dynamic_extent>;
	using bounded64 = bitset<std::uint64_t, dynamic_extent, 4>; // capacity: 4 segments = 256 bits

	TEST_CASE("static properties") {
		static_assert(!dyn64::has_storage_limit);
		static_assert(dyn64::segment_size == sizeof(std::uint64_t));
		static_assert(dyn64::segment_size_in_bits == 64);

		static_assert(bounded64::has_storage_limit);
		static_assert(bounded64::storage_size == 4 * sizeof(std::uint64_t));
		static_assert(bounded64::storage_size_in_bits == 4 * 64);
	}

	TEST_CASE("construction") {
		SUBCASE("initializer list") {
			dyn8 b{0b00000001, 0b10000000};
			REQUIRE_EQ(b.size(), 2);
			REQUIRE_EQ(b.inner_size(), sizeof(std::uint8_t));
			REQUIRE_EQ(b.inner_size_in_bits(), 8);
			REQUIRE_EQ(b.size_in_bits(), 16);

			CHECK(b.test(0));
			for (size_t i = 1; i < 7; ++i) {
				CHECK_FALSE(b.test(i));
			}
			CHECK_FALSE(b.test(7));
			for (size_t i = 8; i < 15; ++i) {
				CHECK_FALSE(b.test(i));
			}
			CHECK(b.test(15));
		}

		SUBCASE("Set fill constructor sets every bit") {
			dyn64 b{dyn64::mode_set, 3};
			REQUIRE_EQ(b.size(), 3);
			for (size_t i = 0; i < b.size_in_bits(); ++i) {
				CHECK(b.test(i));
			}
		}

		SUBCASE("Unset fill constructor clears every bit") {
			dyn64 b{dyn64::mode_unset, 3};
			REQUIRE_EQ(b.size(), 3);
			for (size_t i = 0; i < b.size_in_bits(); ++i) {
				CHECK_FALSE(b.test(i));
			}
		}

		SUBCASE("create_bitset helper") {
			auto b = create_bitset({0b101uz, 0b010uz});
			CHECK(b.test(0));
			CHECK_FALSE(b.test(1));
			CHECK(b.test(2));
			CHECK_EQ(b.count(), 3);
		}

		SUBCASE("copy/move construction and assignment are independent of the source") {
			dyn8 a{0x00, 0x00};

			dyn8 b{a};
			b.set(3);
			CHECK_FALSE(a.test(3));
			CHECK(b.test(3));

			dyn8 c{std::move(b)};
			CHECK(c.test(3));

			dyn8 d{0xFF};
			d = a;
			CHECK_FALSE(d.test(3));
			CHECK_EQ(d.size(), 2);
		}
	}

	TEST_CASE("create_bitset overloads") {
		SUBCASE("dynamic size, dynamic capacity, size_t segments") {
			auto b = create_bitset({0b101uz, 0b010uz});
			static_assert(std::is_same_v<decltype(b), bitset<std::size_t, dynamic_extent, dynamic_extent>>);
			CHECK_EQ(b.count(), 3);
		}

		SUBCASE("dynamic size, bounded capacity, size_t segments") {
			auto b = create_bitset<4>({0b101uz, 0b010uz});
			static_assert(std::is_same_v<decltype(b), bitset<std::size_t, dynamic_extent, 4>>);
			CHECK_EQ(b.count(), 3);
		}

		SUBCASE("dynamic size, bounded capacity, custom segment type") {
			auto b = create_bitset<std::uint8_t, 4>({0b101, 0b010});
			static_assert(std::is_same_v<decltype(b), bitset<std::uint8_t, dynamic_extent, 4>>);
			CHECK_EQ(b.count(), 3);
		}

		SUBCASE("dynamic size, dynamic capacity, custom segment type") {
			auto b = create_bitset<std::uint8_t>({0b101, 0b010});
			static_assert(std::is_same_v<decltype(b), bitset<std::uint8_t, dynamic_extent, dynamic_extent>>);
			CHECK_EQ(b.count(), 3);
		}
	}

	TEST_CASE("bit manipulation across segments") {
		SUBCASE("set/unset/flip/test grows storage automatically") {
			dyn64 b{};
			b.set(5);
			REQUIRE_EQ(b.size(), 1);
			CHECK(b.test(5));
			CHECK_FALSE(b.test(0));
			CHECK_FALSE(b.test(63));

			b.set(130); // segment 2, offset 2 -> needs 3 segments total
			REQUIRE_EQ(b.size(), 3);
			CHECK(b.test(130));

			b.flip(130);
			CHECK_FALSE(b.test(130));
			b.flip(130);
			CHECK(b.test(130));

			b.unset(130);
			CHECK_FALSE(b.test(130));
			b.unset(5);
			CHECK_FALSE(b.test(5));
		}

		SUBCASE("growing to exactly a segment boundary") {
			// setting bit 64 (the first bit of the 2nd 64-bit segment) on an empty bitset must
			// grow storage to 2 segments, not 1 - the required segment count needs to account for
			// the fact that bit index 64 needs capacity > 64 bits, not merely >= 64 bits.
			dyn64 b{};
			b.set(64);
			REQUIRE_EQ(b.size(), 2);
			CHECK(b.test(64));
			CHECK_FALSE(b.test(0));
			CHECK_FALSE(b.test(63));
		}

		SUBCASE("out_of_range when index exceeds a fixed capacity") {
			bounded64 b{};
			REQUIRE_THROWS_AS(b.set(bounded64::storage_size_in_bits), std::out_of_range);
			REQUIRE_THROWS_AS(b.test(bounded64::storage_size_in_bits), std::out_of_range);
			CHECK_NOTHROW(b.set(bounded64::storage_size_in_bits - 1));
		}
	}

	TEST_CASE("fully static capacity (extent == segments)") {
		using fixed64 = bitset<std::uint64_t, 4, 4>;

		SUBCASE("initializer list must match extent exactly") {
			fixed64 b{1, 2, 3, 4};
			REQUIRE_EQ(b.size(), 4);
			CHECK(b.test(0));   // segment 0, value 1
			CHECK(b.test(65));  // segment 1, offset 1, value 2
			CHECK(b.test(129)); // segment 2, offset 1, value 3
		}

		SUBCASE("mismatched initializer list size throws") {
			CHECK_THROWS_AS((fixed64{1, 2, 3}), std::length_error);
		}

		SUBCASE("out_of_range at the fixed capacity boundary") {
			fixed64 b{0, 0, 0, 0};
			REQUIRE_THROWS_AS(b.set(fixed64::storage_size_in_bits), std::out_of_range);
			CHECK_NOTHROW(b.set(fixed64::storage_size_in_bits - 1));
		}

		SUBCASE("set_first_free reports full once every bit is set") {
			fixed64 b{~0ull, ~0ull, ~0ull, ~0ull};
			CHECK_EQ(b.set_first_free(), fixed64::storage_size_in_bits);
		}
	}

	TEST_CASE("count") {
		SUBCASE("popcount across multiple segments") {
			dyn8 b{0xFF, 0x0F, 0x00, 0x01};
			CHECK_EQ(b.count(), 8 + 4 + 0 + 1);
		}

		SUBCASE("empty bitset has count zero") {
			dyn8 b{};
			CHECK_EQ(b.count(), 0);
		}
	}

	TEST_CASE("bit counting within a single segment") {
		struct case_t {
			std::uint8_t value;
			std::size_t countr_zero;
			std::size_t countl_zero;
			std::size_t countr_one;
			std::size_t countl_one;
		};

		std::array<case_t, 4> const cases{
			case_t{0b00000000, 8, 8, 0, 0},
			case_t{0b11111111, 0, 0, 8, 8},
			case_t{0b00001000, 3, 4, 0, 0},
			case_t{0b11110111, 0, 0, 3, 4}, // bit 3 unset: 3 trailing ones, 4 leading ones
		};

		for (auto const &c : cases) {
			CAPTURE(static_cast<unsigned>(c.value));
			dyn8 b{c.value};
			CHECK_EQ(b.countr_zero(), c.countr_zero);
			CHECK_EQ(b.countl_zero(), c.countl_zero);
			CHECK_EQ(b.countr_one(), c.countr_one);
			CHECK_EQ(b.countl_one(), c.countl_one);
		}
	}

	TEST_CASE("bit counting across multiple segments") {
		// segment index order is low-to-high; global bit index 0 (segment 0, offset 0) is the
		// least-significant end (segment_set uses "1 << offset", i.e. offset 0 is the LSB within
		// a segment) and the highest segment holds the most-significant bits.

		SUBCASE("countr_zero accumulates over fully-zero low segments") {
			dyn8 b{0x00, 0x00, 0b00000100};
			CHECK_EQ(b.countr_zero(), 8 + 8 + 2);
		}

		SUBCASE("countr_one accumulates over fully-one low segments") {
			dyn8 b{0xFF, 0xFF, 0b11111011};
			CHECK_EQ(b.countr_one(), 8 + 8 + 2);
		}

		SUBCASE("countl_zero accumulates over fully-zero high segments") {
			// segment 0 has its top bit set (no leading zeros locally); segments 1 and 2 sit at
			// the high/most-significant end and are fully zero, so the global leading-zero count
			// must include both of them before reaching segment 0's own contribution of 0.
			dyn8 b{0b10000000, 0x00, 0x00};
			CHECK_EQ(b.countl_zero(), 8 + 8 + 0);
		}

		SUBCASE("countl_one accumulates over fully-one high segments") {
			dyn8 b{0b01111111, 0xFF, 0xFF};
			CHECK_EQ(b.countl_one(), 8 + 8 + 0);
		}
	}

	TEST_CASE("all_set / any_set / none_set") {
		SUBCASE("all_set true only when every bit is 1") {
			dyn8 full{0xFF, 0xFF};
			dyn8 partial{0xFF, 0xFE};
			dyn8 empty_bits{0x00, 0x00};

			CHECK(full.all_set());
			CHECK_FALSE(partial.all_set());
			CHECK_FALSE(empty_bits.all_set());
		}

		SUBCASE("none_set true only when every bit is 0") {
			dyn8 empty_bits{0x00, 0x00};
			dyn8 partial{0x00, 0x01};
			dyn8 full{0xFF, 0xFF};

			CHECK(empty_bits.none_set());
			CHECK_FALSE(partial.none_set());
			CHECK_FALSE(full.none_set());
		}

		SUBCASE("any_set true when a fully-set segment is present") {
			dyn8 b{0xFF, 0x00};
			CHECK(b.any_set());
		}

		SUBCASE("any_set true when only a non-zero-offset bit is set") {
			// bit 0 is unset but bit 1 is set - any_set must still report true
			dyn8 b{0b00000010};
			CHECK(b.any_set());
		}

		SUBCASE("any_set false for a fully empty bitset") {
			dyn8 b{0x00, 0x00};
			CHECK_FALSE(b.any_set());
		}

		SUBCASE("any_set true for a fully mixed bitset") {
			dyn8 b{0b01010101};
			CHECK(b.any_set());
		}
	}

	TEST_CASE("set_first_free") {
		SUBCASE("finds first unset bit within existing segments") {
			dyn8 b{0xFF, 0b00000001};
			auto const ix = b.set_first_free();
			CHECK_EQ(ix, 9); // segment 1, offset 1 -> global index 8 + 1
			CHECK(b.test(9));
		}

		SUBCASE("grows storage when all existing segments are full") {
			dyn8 b{0xFF};
			auto const ix = b.set_first_free();
			CHECK_EQ(ix, 8); // new segment appended, first bit of it
			REQUIRE_EQ(b.size(), 2);
			CHECK(b.test(8));
		}

		SUBCASE("reports storage_size_in_bits once a fixed-capacity bitset is full") {
			bounded64 b{~0ull, ~0ull, ~0ull, ~0ull};
			CHECK_EQ(b.set_first_free(), bounded64::storage_size_in_bits);
		}
	}

	TEST_CASE("equality") {
		SUBCASE("equal bitsets compare equal") {
			dyn8 a{0x12, 0x34};
			dyn8 b{0x12, 0x34};
			CHECK(a == b);
		}

		SUBCASE("differing content compares unequal") {
			dyn8 a{0x12, 0x34};
			dyn8 b{0x12, 0x35};
			CHECK_FALSE(a == b);
		}

		SUBCASE("differing size compares unequal") {
			dyn8 a{0x12};
			dyn8 b{0x12, 0x00};
			CHECK_FALSE(a == b);
		}
	}

	TEST_CASE("bitwise combination") {
		SUBCASE("operator&= / operator&") {
			dyn8 a{0b11001100};
			dyn8 b{0b10101010};
			auto const c = a & b; // -> 0b10001000

			CHECK_EQ(c.count(), 2);
			a &= b;
			CHECK(a == c);
		}

		SUBCASE("operator|= / operator|") {
			dyn8 a{0b11001100};
			dyn8 b{0b10101010};
			auto const c = a | b; // -> 0b11101110

			CHECK_EQ(c.count(), 6);
			a |= b;
			CHECK(a == c);
		}
	}

	TEST_CASE("iteration") {
		SUBCASE("bit-mode iteration visits every bit exactly once") {
			dyn8 b{0x00, 0x00};
			auto it = b.begin();
			auto const sentinel = b.end();

			size_t visited = 0;
			while (it != sentinel) {
				++visited;
				++it;
			}
			CHECK_EQ(visited, b.size_in_bits());
		}

		SUBCASE("operator* reads the bit at the iterator's current position") {
			dyn8 b{0b00000101};
			auto it = b.begin();
			CHECK(*it); // bit 0
			++it;
			CHECK_FALSE(*it); // bit 1
			++it;
			CHECK(*it); // bit 2
		}

		SUBCASE("writing through the iterator itself (not *it) sets the underlying bit") {
			dyn8 b{0x00};
			auto it = b.begin();
			it = true;
			CHECK(b.test(0));

			++it;
			it = true;
			CHECK(b.test(1));

			it = false;
			CHECK_FALSE(b.test(1));
		}

		SUBCASE("segment-mode increment advances by a whole segment") {
			dyn8 b{0xAA, 0xBB};
			auto it = b.begin();
			CHECK_EQ(it.get(), 0xAA);
			it.operator++<bitset_const::segment_mode>();
			CHECK_EQ(it.get(), 0xBB);
		}

		SUBCASE("operator+= / operator+ skip ahead by bits or by segments") {
			dyn8 b{0b00000000, 0b00000010};

			auto it = b.begin();
			it += 9; // segment 1, offset 1
			CHECK(*it);

			auto const it2 = b.begin() + 9;
			CHECK(*it2);

			auto it3 = b.begin();
			it3.operator+=<bitset_const::segment_mode>(1);
			CHECK_EQ(it3.get(), 0b00000010);
		}
	}

	TEST_CASE("shifts") {
		SUBCASE("operator<<= moves bits toward lower indices and clears vacated bits") {
			dyn8 b{0b10110011}; // bit(i): 1,1,0,0,1,1,0,1 for i = 0..7
			b <<= 2;
			bool const expected[8] = {false, false, true, true, false, true, false, false};
			for (size_t i = 0; i < 8; ++i) {
				CHECK_EQ(b.test(i), expected[i]);
			}
		}

		SUBCASE("operator>>= moves bits toward higher indices and clears vacated bits") {
			dyn8 b{0b10110011};
			b >>= 2;
			bool const expected[8] = {false, false, true, true, false, false, true, true};
			for (size_t i = 0; i < 8; ++i) {
				CHECK_EQ(b.test(i), expected[i]);
			}
		}

		SUBCASE("operator<< / operator>> produce a shifted copy, leaving the original untouched") {
			dyn8 b{0b10110011}; // bit(i): 1,1,0,0,1,1,0,1 for i = 0..7
			auto const left = b << 2;
			auto const right = b >> 2;

			bool const expected_left[8] = {false, false, true, true, false, true, false, false};
			bool const expected_right[8] = {false, false, true, true, false, false, true, true};
			for (size_t i = 0; i < 8; ++i) {
				CHECK_EQ(left.test(i), expected_left[i]);
				CHECK_EQ(right.test(i), expected_right[i]);
			}

			// original untouched by either shift
			CHECK(b.test(0));
			CHECK(b.test(1));
			CHECK_FALSE(b.test(2));
		}
	}

	TEST_CASE("formatting") {
		// the formatter's output isn't specified/stable enough to assert on - just make sure the
		// various format specs run without throwing, and print the result for a human to eyeball.
		dyn8 b{0b10110011, 0x00};
		MESSAGE("default: ", std::format("{}", b));
		MESSAGE("hex: ", std::format("{:x}", b));
		MESSAGE("binary: ", std::format("{:b}", b));
		MESSAGE("debug+hex: ", std::format("{:?x}", b));
		MESSAGE("debug+binary: ", std::format("{:?b}", b));
	}

	TEST_CASE("multi-word (non-integral) segment type") {
		using wide_bitset = bitset<wide_word, dynamic_extent, dynamic_extent>;

		SUBCASE("single-bit addressing within a multi-word segment") {
			// bit 10 belongs to the low 64-bit word of the segment and must read back as set.
			// (chosen small enough that a wrong chunk computation still lands in-bounds instead
			// of reading/writing past the segment.)
			wide_word w{};
			w.lo = 1ull << 10;

			wide_bitset b{w};
			CHECK(b.test(10));
		}

		SUBCASE("count / countr_zero span both 64-bit words") {
			wide_word w{};
			w.lo = 0b1011;                // 3 bits set in the low word
			w.hi = 0x8000000000000000ull; // top bit of the high word set

			wide_bitset b{w};
			CHECK_EQ(b.count(), 4);
			CHECK_EQ(b.countr_zero(), 0); // bit 0 of the low word is already set
		}

		SUBCASE("segment_free based queries (any_set / none_set)") {
			wide_bitset empty_b{wide_word{}};
			CHECK(empty_b.none_set());
			CHECK_FALSE(empty_b.any_set());

			wide_word w{};
			w.hi = 1; // only bit 64 set
			wide_bitset b{w};
			CHECK(b.any_set());
			CHECK_FALSE(b.none_set());
		}
	}
}
