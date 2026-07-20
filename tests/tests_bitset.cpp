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
			REQUIRE_EQ(b.size(), 2);
			CHECK_EQ(b.count(), 3);
			CHECK(b.test(0));
			CHECK_FALSE(b.test(1));
			CHECK(b.test(2));
			CHECK(b.test(decltype(b)::segment_size_in_bits + 1)); // segment 1, bit 1 (value 0b010)
		}

		SUBCASE("dynamic size, bounded capacity, size_t segments") {
			auto b = create_bitset<4>({0b101uz, 0b010uz});
			static_assert(std::is_same_v<decltype(b), bitset<std::size_t, dynamic_extent, 4>>);
			REQUIRE_EQ(b.size(), 2);
			CHECK_EQ(b.count(), 3);
			CHECK(b.test(0));
			CHECK(b.test(2));
			CHECK(b.test(decltype(b)::segment_size_in_bits + 1));

			CHECK_THROWS_AS((create_bitset<4>({0uz, 0uz, 0uz, 0uz, 0uz})), std::length_error);
		}

		SUBCASE("dynamic size, bounded capacity, custom segment type") {
			auto b = create_bitset<std::uint8_t, 4>({0b101, 0b010});
			static_assert(std::is_same_v<decltype(b), bitset<std::uint8_t, dynamic_extent, 4>>);
			REQUIRE_EQ(b.size(), 2);
			CHECK_EQ(b.count(), 3);
			CHECK(b.test(0));
			CHECK(b.test(2));
			CHECK(b.test(decltype(b)::segment_size_in_bits + 1)); // segment 1, bit 1

			CHECK_THROWS_AS((create_bitset<std::uint8_t, 4>({0, 0, 0, 0, 0})), std::length_error);
		}

		SUBCASE("dynamic size, dynamic capacity, custom segment type") {
			auto b = create_bitset<std::uint8_t>({0b101, 0b010});
			static_assert(std::is_same_v<decltype(b), bitset<std::uint8_t, dynamic_extent, dynamic_extent>>);
			REQUIRE_EQ(b.size(), 2);
			CHECK_EQ(b.count(), 3);
			CHECK(b.test(0));
			CHECK(b.test(2));
			CHECK(b.test(decltype(b)::segment_size_in_bits + 1));
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
			REQUIRE_THROWS_AS((void)b.test(bounded64::storage_size_in_bits), std::out_of_range);
			CHECK_EQ(b.size(), 0); // rejected out-of-range set() must not have grown anything
			CHECK_NOTHROW(b.set(bounded64::storage_size_in_bits - 1));
		}

		SUBCASE("a large global index computes the exact number of segments needed and zero-fills the rest") {
			// regression: growth must be driven off the actual bit index (which can require far
			// more than one extra segment), not just the "next segment" case exercised above.
			dyn64 b{};
			b.set(1000); // segment 15 (1000 / 64), so 16 segments are required in total
			REQUIRE_EQ(b.size(), 16);
			CHECK(b.test(1000));

			for (size_t i = 0; i < b.size_in_bits(); ++i) {
				if (i == 1000) continue;
				CAPTURE(i);
				CHECK_FALSE(b.test(i));
			}
		}

		SUBCASE("setting a lower index after a large-index growth does not grow further") {
			dyn64 b{};
			b.set(1000);
			REQUIRE_EQ(b.size(), 16);

			b.set(0);
			CHECK_EQ(b.size(), 16); // already had enough room, must not have resized again
			CHECK(b.test(0));
			CHECK(b.test(1000));
		}

		SUBCASE("bounded capacity auto-grows its logical size, staying consistent with count/iteration") {
			// regression: set() on a bounded bitset used to write the bit directly without ever
			// growing size(), so test() would report the bit as set while count()/iteration/
			// all_set()/etc (which all iterate up to size()) stayed blind to it entirely.
			bounded64 b{};
			REQUIRE_EQ(b.size(), 0);

			b.set(5);
			CHECK_EQ(b.size(), 1);
			CHECK(b.test(5));
			CHECK_EQ(b.count(), 1);

			size_t visited = 0;
			for (auto it = b.begin(); it != b.end(); ++it) ++visited;
			CHECK_EQ(visited, b.size_in_bits());
		}

		SUBCASE("bounded capacity growth stops exactly at its static maximum and stays zero-filled") {
			bounded64 b{};
			b.set(bounded64::storage_size_in_bits - 1); // last valid bit, far beyond current size()
			REQUIRE_EQ(b.size(), 4);
			CHECK(b.test(bounded64::storage_size_in_bits - 1));
			CHECK_EQ(b.count(), 1); // every other (newly exposed) bit must be zero, not garbage
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

		SUBCASE("operator-- / operator--(int) walk backwards, wrapping across segments") {
			dyn8 b{0b00000001, 0b10000000};

			auto it = b.begin() + 9; // segment 1, offset 1
			--it;                    // segment 1, offset 0
			CHECK_EQ(it.get(), 0b10000000);

			--it; // wraps back into segment 0, offset 7 (top bit of segment 0)
			CHECK_EQ(it.get(), 0b00000001);
			CHECK_FALSE(*it); // offset 7 of segment 0 is unset

			auto it2 = b.begin() + 3;
			auto const prev = it2--;
			CHECK(prev == b.begin() + 3);
			CHECK(it2 == b.begin() + 2);
		}

		SUBCASE("operator-= / operator- walk backwards by bits or by segments") {
			dyn8 b{0b00000000, 0b00000010};

			auto it = b.begin() + 9;
			it -= 9;
			CHECK_FALSE(*it);
			CHECK(it == b.begin());

			auto const it2 = (b.begin() + 9) - 9;
			CHECK(it2 == b.begin());

			dyn8 b2{0xAA, 0xBB};
			auto it3 = b2.begin();
			it3.operator++<bitset_const::segment_mode>();
			CHECK_EQ(it3.get(), 0xBB);
			it3.operator--<bitset_const::segment_mode>();
			CHECK_EQ(it3.get(), 0xAA);
		}

		SUBCASE("const bitset yields a const_iterator with read access") {
			dyn8 const b{0b00000101};
			auto it = b.begin();
			CHECK(*it);
			CHECK_EQ(it.get(), 0b00000101);
		}

		SUBCASE("bit_ref proxy assignment (through operator*, not just the iterator itself)") {
			dyn8 b{0x00};
			auto it = b.begin();
			*it = true;
			CHECK(b.test(0));

			++it;
			*it = true;
			CHECK(b.test(1));

			*it = false;
			CHECK_FALSE(b.test(1));
		}

		SUBCASE("explicit iterator construction with an offset validates the offset") {
			dyn8 b{0x00};
			CHECK_NOTHROW((dyn8::iterator{b, 7}));
			CHECK_THROWS_AS((dyn8::iterator{b, 8}), std::out_of_range); // segment_size_in_bits == 8

			dyn8::iterator it{b, 3};
			CHECK(it == b.begin() + 3);
		}

		SUBCASE("explicit iterator construction with an offset and segment validates both") {
			dyn8 b{0x00, 0x00};
			CHECK_NOTHROW((dyn8::iterator{b, 0, 1}));
			CHECK_THROWS_AS((dyn8::iterator{b, 8, 0}), std::out_of_range);  // bad offset
			CHECK_THROWS_AS((dyn8::iterator{b, 0, 2}), std::out_of_range); // segment out of bounds, size() == 2

			dyn8::iterator it{b, 2, 1};
			CHECK(it == b.begin() + 10);
		}

		SUBCASE("reverse iteration (rbegin/rend) visits every bit in reverse order exactly once") {
			dyn8 b{0b00000001, 0b10000000};

			size_t visited = 0;
			int global_ix = 15;
			for (auto it = b.rbegin(); it != b.rend(); ++it, --global_ix) {
				bool const expected = (global_ix == 0 || global_ix == 15);
				CHECK_EQ(static_cast<bool>(*it), expected);
				++visited;
			}
			CHECK_EQ(visited, b.size_in_bits());
			CHECK_EQ(global_ix, -1);
		}

		SUBCASE("reverse iteration on a const bitset") {
			dyn8 const b{0b00000001, 0b10000000};
			size_t visited = 0;
			for (auto it = b.rbegin(); it != b.rend(); ++it) {
				++visited;
			}
			CHECK_EQ(visited, b.size_in_bits());
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

		SUBCASE("shift by 0 is a no-op") {
			dyn8 b{0b10110011};
			b <<= 0;
			CHECK_EQ(b.count(), 5);
			CHECK(b.test(0));
			CHECK(b.test(7));

			b >>= 0;
			CHECK_EQ(b.count(), 5);
		}

		SUBCASE("shift by exactly size_in_bits() clears everything") {
			dyn8 b{0xFF, 0xFF};
			b <<= b.size_in_bits();
			CHECK_EQ(b.count(), 0);

			dyn8 b2{0xFF, 0xFF};
			b2 >>= b2.size_in_bits();
			CHECK_EQ(b2.count(), 0);
		}

		SUBCASE("shift by more than size_in_bits() clears everything (regression: used to hang)") {
			// previously, shifting by more bits than the bitset holds caused the internal
			// iterator arithmetic to compare against the static storage capacity instead of
			// the bitset's actual size, so the move/fill loop in operator<<=/>>= never
			// terminated for bitsets whose size() can be less than their max capacity.
			dyn8 b{0xFF, 0xFF};
			b <<= b.size_in_bits() + 5;
			CHECK_EQ(b.count(), 0);

			dyn8 b2{0xFF, 0xFF};
			b2 >>= b2.size_in_bits() + 5;
			CHECK_EQ(b2.count(), 0);

			bounded64 b3{~0ull, ~0ull}; // size() == 2 segments, capacity 4 segments
			b3 <<= b3.size_in_bits() + 30; // within capacity, beyond current size
			CHECK_EQ(b3.count(), 0);

			bounded64 b4{~0ull, ~0ull};
			b4 <<= bounded64::storage_size_in_bits + 100; // beyond even the max capacity
			CHECK_EQ(b4.count(), 0);
		}

		SUBCASE("shift crossing multiple segment boundaries") {
			dyn8 left{0x00, 0x00, 0xFF}; // bits 16..23 set (top segment)
			left <<= 10;                 // pulls the high segment's bits down across a boundary
			CHECK_FALSE(left.test(5));
			CHECK(left.test(6));
			CHECK(left.test(13));
			CHECK_FALSE(left.test(14));

			dyn8 right{0xFF, 0x00, 0x00}; // bits 0..7 set (bottom segment)
			right >>= 10;                 // pushes the low segment's bits up across a boundary
			CHECK_FALSE(right.test(9));
			CHECK(right.test(10));
			CHECK(right.test(17));
			CHECK_FALSE(right.test(18));
		}
	}

	TEST_CASE("all_set / any_set / none_set on an empty bitset") {
		dyn8 b{};
		REQUIRE_EQ(b.size(), 0);
		CHECK(b.all_set());   // vacuously true: no bit fails to be set
		CHECK_FALSE(b.any_set());
		CHECK(b.none_set());  // vacuously true: no bit is set
	}

	TEST_CASE("bitwise combination with mismatched sizes leaves the receiver untouched") {
		// segment_handler(bitset const&) bails out (returns false) as soon as it sees a size
		// mismatch, before touching any segment - documenting that &=/|= are silent no-ops here
		// rather than throwing, unlike e.g. set()/test() which throw on out-of-range access.
		SUBCASE("operator&= with a differently-sized operand") {
			dyn8 a{0xFF, 0xFF};
			dyn8 b{0xFF};
			a &= b;
			CHECK_EQ(a.count(), 16);
		}

		SUBCASE("operator|= with a differently-sized operand") {
			dyn8 a{0x00, 0x00};
			dyn8 b{0xFF};
			a |= b;
			CHECK_EQ(a.count(), 0);
		}
	}

	TEST_CASE("fixed and bounded capacity - equality, bitwise ops and shifts") {
		using fixed64 = bitset<std::uint64_t, 4, 4>;

		SUBCASE("fully static capacity supports equality and bitwise combination") {
			fixed64 a{0b1100, 0, 0, 0};
			fixed64 b{0b1010, 0, 0, 0};

			auto const c = a & b;
			CHECK_EQ(c.count(), 1);
			auto const d = a | b;
			CHECK_EQ(d.count(), 3);
			CHECK_FALSE(a == b);

			fixed64 a_copy = a;
			CHECK(a == a_copy);
		}

		SUBCASE("fully static capacity supports shifting") {
			fixed64 a{0b1100, 0, 0, 0};
			a <<= 1;
			CHECK_FALSE(a.test(0));
			CHECK(a.test(1));
			CHECK(a.test(2));
			CHECK_FALSE(a.test(3));
		}

		SUBCASE("bounded capacity supports equality and bitwise combination") {
			bounded64 a{0b1100, 0};
			bounded64 b{0b1010, 0};

			auto const c = a & b;
			CHECK_EQ(c.count(), 1);
			auto const d = a | b;
			CHECK_EQ(d.count(), 3);
			CHECK_FALSE(a == b);
		}

		SUBCASE("bounded capacity initializer list exceeding max_size throws length_error") {
			CHECK_THROWS_AS((bounded64{1, 2, 3, 4, 5}), std::length_error);
			CHECK_NOTHROW((bounded64{1, 2, 3, 4}));
		}

		SUBCASE("bounded capacity set_first_free grows by one segment without hitting the limit") {
			bounded64 b{~0ull, ~0ull}; // 2 of 4 segments used, both full
			auto const ix = b.set_first_free();
			CHECK_EQ(ix, bounded64::segment_size_in_bits * 2); // first bit of the freshly grown 3rd segment
			REQUIRE_EQ(b.size(), 3);
			CHECK(b.test(ix));
		}
	}

	TEST_CASE("formatting") {
		// the formatter's output isn't specified/stable enough to assert on - just make sure the
		// various format specs run without throwing, and print the result for a human to eyeball.
		dyn8 b{0b10110011, 0x00};
		MESSAGE("hex: ", std::format("{:x}", b));
		MESSAGE("binary: ", std::format("{:b}", b));
		MESSAGE("debug+hex: ", std::format("{:?x}", b));
		MESSAGE("debug+binary: ", std::format("{:?b}", b));
	}

    TEST_CASE("formatting long") {
	    dyn64 b_long{dyn64::mode_unset, 32};
	    MESSAGE("hex: ", std::format("{:x}", b_long));
	    MESSAGE("binary: ", std::format("{:b}", b_long));
	    MESSAGE("debug+hex: ", std::format("{:?x}", b_long));
	    MESSAGE("debug+binary: ", std::format("{:?b}", b_long));
	}

	TEST_CASE("formatting edge cases") {
		SUBCASE("no format spec just steps through without hex/binary rendering") {
			dyn8 b{0b10110011};
			std::string s;
			CHECK_NOTHROW(s = std::format("{}", b));
		}

		SUBCASE("debug spec alone (no hex/binary) is accepted") {
			dyn8 b{0b10110011};
			std::string s;
			CHECK_NOTHROW(s = std::format("{:?}", b));
		}

		SUBCASE("an unrecognized format spec character throws format_error") {
			// the spec is validated at compile time for a literal format string, so a runtime
			// format string is used here to actually exercise the throwing parse() path.
			dyn8 b{0x00};
			std::string s;
			CHECK_THROWS_AS(s = std::vformat("{:z}", std::make_format_args(b)), std::format_error);
		}
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

		SUBCASE("bit counting spans multiple multi-word segments") {
			// segment 0 (lowest) is fully set, segment 1 (highest) is fully zero - mirrors the
			// "bit counting across multiple segments" integral tests, but for a segment type
			// wide enough to need segment_slots/slot_handler internally.
			wide_word full{};
			full.lo = ~0ull;
			full.hi = ~0ull;
			wide_word zero{};

			wide_bitset b{full, zero};
			CHECK_FALSE(b.all_set());
			CHECK_EQ(b.countr_one(), 128);
			CHECK_EQ(b.countl_zero(), 128);
			CHECK_EQ(b.countl_one(), 0);

			wide_bitset all_full{full, full};
			CHECK(all_full.all_set());
		}

		SUBCASE("operator&= / operator|= / operator== on multi-word segments") {
			wide_word a_word{};
			a_word.lo = 0b1100;
			wide_word b_word{};
			b_word.lo = 0b1010;

			wide_bitset a{a_word};
			wide_bitset b{b_word};

			auto const conjunction = a & b;
			CHECK_EQ(conjunction.count(), 1);
			auto const disjunction = a | b;
			CHECK_EQ(disjunction.count(), 3);

			CHECK(a == a);
			CHECK_FALSE(a == b);

			a &= b;
			CHECK(a == conjunction);
		}

		SUBCASE("segment_slots exposes every word making up one segment") {
			wide_word w{};
			w.lo = 5;
			w.hi = 9;

			wide_bitset b{w};
			auto it = b.begin();
			auto [word, end] = b.segment_slots(it.get());
			CHECK_EQ(std::distance(word, end), wide_bitset::segment_steps);
			CHECK_EQ(*word, 5);
			CHECK_EQ(*(word + 1), 9);
		}

		SUBCASE("formatting does not throw for a multi-word segment type") {
			wide_word w{};
			w.lo = 0x1234;
			w.hi = 0x5678;
			wide_bitset b{w};
			std::string s;
			CHECK_NOTHROW(s = std::format("{:x}", b));
			CHECK_NOTHROW(s = std::format("{:?x}", b));
		}
	}

	TEST_CASE("integral segment width coverage") {
		// verifies storage_word selection (and the derived set/test/count/countr_zero paths)
		// across differently-sized plain integral segment types, not just uint8_t/uint64_t.
		auto const check_widths = []<typename T>() {
			using B = bitset<T, dynamic_extent, dynamic_extent>;
			B b{};
			b.set(3);
			b.set(B::segment_size_in_bits + 1); // force growth into a 2nd segment

			CHECK_EQ(b.size(), 2);
			CHECK_EQ(b.count(), 2);
			CHECK(b.test(3));
			CHECK(b.test(B::segment_size_in_bits + 1));
			CHECK_EQ(b.countr_zero(), 3);
		};

		SUBCASE("uint16_t segments") {
			check_widths.operator()<std::uint16_t>();
		}

		SUBCASE("uint32_t segments") {
			check_widths.operator()<std::uint32_t>();
		}

#ifdef __SIZEOF_INT128__
		SUBCASE("__uint128_t segments") {
			check_widths.operator()<__uint128_t>();
		}
#endif
	}
}
