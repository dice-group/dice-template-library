#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include <dice/template-library/bitset.hpp>

#include <array>
#include <bitset>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <dice/template-library/sandbox.hpp>

#include <boost/dynamic_bitset.hpp>

#include <algorithm>
#include <cstddef>
#include <format>
#include <new>
#include <random>
#include <string>
#include <string_view>
#include <vector>

// helper for the position_iterator concept checks below: requires-expressions need a genuine
// template parameter to fail via substitution rather than a hard compile error, so this can't be
// inlined as `requires(dyn64::positional_iterator<...> it, ...) { it[n]; }` inside the static_assert.
template<typename I>
concept has_subscript_operator = requires(I i, std::iter_difference_t<I> n) { i[n]; };

// position_iterator's dereference is a plain value_type (size_t) now, not a writable proxy -
// verifies there is nothing to assign through it.
template<typename I>
concept dereference_is_assignable = requires(I i, bool b) { *i = b; };

// The initializer-list ctor sizes a bitset to the exact bit width of the list: trailing all-zero
// segments are dropped and the top segment contributes only its significant bits, so `dyn8{0x08}`
// is 4 bits wide and `dyn8{0x00, 0x00}` is empty. Where a test needs a value *and* a logical size
// that fills whole segments, it has to say so - that is what this builds.
template<typename Bitset, typename Value>
Bitset filled(Value const pattern, size_t const bits) {
	Bitset b{};
	b.resize(bits);
	for (size_t i = 0; i < bits; ++i) {
		if ((static_cast<uint64_t>(pattern) >> i) & 1u) {
			b.set(i);
		}
	}
	return b;
}

// a bitset of `bits` zero bits - the storage an all-zero initializer list used to allocate
template<typename Bitset>
Bitset zeroed(size_t const bits) {
	Bitset b{};
	b.resize(bits);
	return b;
}

TEST_SUITE("bitset") {
	using namespace dice::template_library;

	using dyn64 = bitset<dynamic_extent, dynamic_extent>;
	using dyn8 = bitset<std::dynamic_extent, dynamic_extent, uint8_t>;
	using bounded64 = bitset<dynamic_extent, 4 * 64>; // capacity: 4 segments = 256 bits (2nd param is now a bit count)

	// segment_size_in_bits/storage_size_in_bits etc. are implementation details and are private,
	// so shape constants that used to come straight from the class are now hardcoded/derived here
	// from the known template arguments instead.
	constexpr size_t bounded64_segment_bits  = 64;
	constexpr size_t bounded64_capacity_bits = 4 * bounded64_segment_bits;

	TEST_CASE("construction") {
		SUBCASE("initializer list") {
			dyn8 b{0b00000001, 0b10000000};
			REQUIRE_EQ(b.capacity_in_bits(), 16);

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

		SUBCASE("size constructor zero-fills every segment") {
			// per the ctor's doc comment ("size to set low"), this only grows storage - it does
			// not set any bits, unlike the old mode_set-based constructor it replaced.
			// NOTE: must use parens, not braces - dyn64{3} would list-initialize a single
			// segment holding the value 3 instead of calling the explicit size_t constructor,
			// since a viable initializer_list constructor always wins list-initialization.
			dyn64 b{};
		    b.resize(3 * 64);
			REQUIRE_EQ(b.capacity_in_bits(), 3 * 64);
			for (size_t i = 0; i < b.capacity_in_bits(); ++i) {
				CHECK_FALSE(b.test(i));
			}
		}

		SUBCASE("copy/move construction and assignment are independent of the source") {
			dyn8 a = zeroed<dyn8>(2 * 8);

			dyn8 b{a};
			b.set(3);
			CHECK_FALSE(a.test(3));
			CHECK(b.test(3));

			dyn8 c{std::move(b)};
			CHECK(c.test(3));

			dyn8 d{0xFF};
			d = a;
			CHECK_FALSE(d.test(3));
			CHECK_EQ(d.capacity_in_bits(), 2 * 8);
		}
	}

	TEST_CASE("bit manipulation across segments") {
		SUBCASE("set/reset/flip/test grows storage automatically") {
			dyn64 b{};
			b.set(5);
			REQUIRE_EQ(b.capacity_in_bits(), 64);
			CHECK(b.test(5));
			CHECK_FALSE(b.test(0));
			CHECK_FALSE(b.test(63));

			b.set(130); // segment 2, offset 2 -> needs 3 segments total
			REQUIRE_EQ(b.capacity_in_bits(), 3 * 64);
			CHECK(b.test(130));

			b.flip(130);
			CHECK_FALSE(b.test(130));
			b.flip(130);
			CHECK(b.test(130));

			b.reset(130);
			CHECK_FALSE(b.test(130));
			b.reset(5);
			CHECK_FALSE(b.test(5));
		}

		SUBCASE("growing to exactly a segment boundary") {
			// setting bit 64 (the first bit of the 2nd 64-bit segment) on an empty bitset must
			// grow storage to 2 segments, not 1 - the required segment count needs to account for
			// the fact that bit index 64 needs capacity > 64 bits, not merely >= 64 bits.
			dyn64 b{};
			b.set(64);
			REQUIRE_EQ(b.capacity_in_bits(), 2 * 64);
			CHECK(b.test(64));
			CHECK_FALSE(b.test(0));
			CHECK_FALSE(b.test(63));
		}

		SUBCASE("out_of_range when index exceeds a fixed capacity") {
			bounded64 b{};
			REQUIRE_THROWS_AS(b.set(bounded64_capacity_bits), std::out_of_range);
			REQUIRE_THROWS_AS((void)b.test(bounded64_capacity_bits), std::out_of_range);
			CHECK_EQ(b.capacity_in_bits(), 0); // rejected out-of-range set() must not have grown anything
			CHECK_NOTHROW(b.set(bounded64_capacity_bits - 1));
		}

		SUBCASE("a large global index computes the exact number of segments needed and zero-fills the rest") {
			// regression: growth must be driven off the actual bit index (which can require far
			// more than one extra segment), not just the "next segment" case exercised above.
			dyn64 b{};
			b.set(1000); // segment 15 (1000 / 64), so 16 segments are required in total
			REQUIRE_EQ(b.capacity_in_bits(), 16 * 64);
			CHECK(b.test(1000));

			for (size_t i = 0; i < b.capacity_in_bits(); ++i) {
				if (i == 1000) continue;
				CAPTURE(i);
				CHECK_FALSE(b.test(i));
			}
		}

		SUBCASE("setting a lower index after a large-index growth does not grow further") {
			dyn64 b{};
			b.set(1000);
			REQUIRE_EQ(b.capacity_in_bits(), 16 * 64);

			b.set(0);
			CHECK_EQ(b.capacity_in_bits(), 16 * 64); // already had enough room, must not have resized again
			CHECK(b.test(0));
			CHECK(b.test(1000));
		}

		SUBCASE("bounded capacity auto-grows its logical size, staying consistent with count/iteration") {
			// regression: set() on a bounded bitset used to write the bit directly without ever
			// growing size(), so test() would report the bit as set while count()/iteration/
			// all_set()/etc (which all iterate up to size()) stayed blind to it entirely.
			bounded64 b{};
			REQUIRE_EQ(b.capacity_in_bits(), 0);

			b.set(5);
			// logical size (size_in_bits()) tracks the exact bit index reached, NOT rounded up to
			// a whole segment - capacity_in_bits() is the (larger) physically-allocated amount.
			CHECK_EQ(b.size_in_bits(), 6);
			CHECK_EQ(b.capacity_in_bits(), 64);
			CHECK(b.test(5));
			CHECK_EQ(b.count(), 1);

			// bit-mode iteration (begin()/end()) is bounded by the exact logical size, not the
			// rounded-up physical capacity - only the 6 logically-used bits are visited.
			size_t visited = 0;
			for (auto it = b.begin(); it != b.end(); ++it) ++visited;
			CHECK_EQ(visited, b.size_in_bits());
		}

		SUBCASE("bounded capacity growth stops exactly at its static maximum and stays zero-filled") {
			bounded64 b{};
			b.set(bounded64_capacity_bits - 1); // last valid bit, right at the maximum
			// setting the very last valid index grows logical size to exactly its maximum, so
			// size_in_bits() and capacity_in_bits() coincide here - not because size_in_bits() is
			// static (it isn't - it's the same dynamic logical size as above), but because this
			// particular index happens to be capacity_in_bits() - 1.
			CHECK_EQ(b.size_in_bits(), bounded64_capacity_bits);
			CHECK_EQ(b.capacity_in_bits(), 4 * 64);
			CHECK(b.test(bounded64_capacity_bits - 1));
			CHECK_EQ(b.count(), 1); // every other (newly exposed) bit must be zero, not garbage
		}
	}

	TEST_CASE("fully static capacity (extent == segments)") {
		using fixed64 = bitset<4 * 64, 4 * 64>; // fixed: pass the same bit count twice
		constexpr size_t fixed64_capacity_bits = 4 * 64;

		SUBCASE("an initializer list of exactly the extent fills every segment") {
			fixed64 b{1, 2, 3, 4};
			REQUIRE_EQ(b.size_in_bits(), 4 * 64);
			CHECK(b.test(0));   // segment 0, value 1
			CHECK(b.test(65));  // segment 1, offset 1, value 2
			CHECK(b.test(129)); // segment 2, offset 1, value 3
		}

		SUBCASE("a shorter initializer list is accepted, the remaining segments stay zero") {
			// the list gives the low segments; a fixed bitset is its declared size either way
			fixed64 b{1, 2, 3};
			CHECK_EQ(b.size_in_bits(), 4 * 64);
			CHECK(b.test(0));
			CHECK(b.test(65));
			CHECK(b.test(129));
			CHECK_EQ(b.count(), 4);  // 1 + 1 + 2 bits, and segment 3 is zero
		}

		SUBCASE("an initializer list wider than the extent throws") {
			CHECK_THROWS_AS((fixed64{1, 2, 3, 4, 5}), std::length_error);
		}

		SUBCASE("out_of_range at the fixed capacity boundary") {
			fixed64 b{0, 0, 0, 0};
			REQUIRE_THROWS_AS(b.set(fixed64_capacity_bits), std::out_of_range);
			CHECK_NOTHROW(b.set(fixed64_capacity_bits - 1));
		}

		SUBCASE("set_first_free reports full once every bit is set") {
			fixed64 b{~0ull, ~0ull, ~0ull, ~0ull};
			CHECK_EQ(b.set_first_free(), fixed64_capacity_bits);
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
			case_t{0b11110111, 0, 0, 3, 4}, // bit 3 reset: 3 trailing ones, 4 leading ones
		};

		for (auto const &c : cases) {
			CAPTURE(static_cast<unsigned>(c.value));
			// the expectations are for 8 logical bits, which the value's own width need not be
			dyn8 const b = filled<dyn8>(c.value, 8);
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
			// an all-zero top segment is trimmed by the ctor, so the size is set explicitly here
			dyn8 b = zeroed<dyn8>(3 * 8);
			b.set(7);
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
			dyn8 zero_bits = zeroed<dyn8>(2 * 8);

			CHECK(full.all_set());
			CHECK_FALSE(partial.all_set());
			CHECK_FALSE(zero_bits.all_set());
		}

		SUBCASE("all_set is vacuously true for a bitset with no bits") {
			// an all-zero initializer list is the empty bitset, and every one of its zero bits is
			// set - same answer std::bitset<0>::all() and boost::dynamic_bitset{}.all() give
			dyn8 const empty_bits{0x00, 0x00};
			REQUIRE_EQ(empty_bits.size_in_bits(), 0);
			CHECK(empty_bits.all_set());
			CHECK_FALSE(empty_bits.any_set());
			CHECK(empty_bits.none_set());
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
			// bit 0 is reset but bit 1 is set - any_set must still report true
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
		SUBCASE("finds first reset bit within existing segments") {
			dyn8 b{0xFF, 0b00000001};
			auto const ix = b.set_first_free();
			CHECK_EQ(ix, 9); // segment 1, offset 1 -> global index 8 + 1
			CHECK(b.test(9));
		}

		SUBCASE("grows storage when all existing segments are full") {
			dyn8 b{0xFF};
			auto const ix = b.set_first_free();
			CHECK_EQ(ix, 8); // new segment appositions_ended, first bit of it
			REQUIRE_EQ(b.capacity_in_bits(), 2 * 8);
			CHECK(b.test(8));
		}

		SUBCASE("reports storage_size_in_bits once a fixed-capacity bitset is full") {
			bounded64 b{~0ull, ~0ull, ~0ull, ~0ull};
			CHECK_EQ(b.set_first_free(), bounded64_capacity_bits);
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
			// identical bits, different logical size - a trailing zero segment would not do it any
			// more, the ctor trims it and both sides would be the same bitset
			dyn8 a{0x12};  // 5 logical bits
			dyn8 b{0x12};
			b.resize(8);
			REQUIRE_EQ(a.count(), b.count());
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

		SUBCASE("operator^= / operator^") {
			dyn8 a{0b11001100};
			dyn8 b{0b10101010};
			auto const c = a ^ b; // -> 0b01100110

			CHECK_EQ(c.count(), 4);
			CHECK_FALSE(c.test(0));
			CHECK(c.test(1));
			CHECK(c.test(2));
			CHECK_FALSE(c.test(3));
			CHECK_FALSE(c.test(4));
			CHECK(c.test(5));
			CHECK(c.test(6));
			CHECK_FALSE(c.test(7));
			a ^= b;
			CHECK(a == c);
		}
	}

	TEST_CASE("operator== / operator!=") {
		dyn8 a{0b11001100};
		dyn8 b{0b11001100};
		dyn8 c{0b10101010};

		CHECK(a == b);
		CHECK_FALSE(a != b);
		CHECK(a != c);
		CHECK_FALSE(a == c);
	}

	TEST_CASE("iteration") {
		SUBCASE("bit-mode iteration visits every bit exactly once") {
			dyn8 b = zeroed<dyn8>(2 * 8);
			auto it = b.begin();
			auto const sentinel = b.end();

			size_t visited = 0;
			while (it != sentinel) {
				++visited;
				++it;
			}
			CHECK_EQ(visited, b.capacity_in_bits());
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

		SUBCASE("operator* converts directly to bool - no static_cast needed") {
			// bitset_iterator::reference has an implicit operator bool(), so the logical type
			// behind the proxy (a single bit) comes out with a plain assignment, not a cast.
			dyn8 b{0b00000101};
			auto it = b.begin();

			bool const bit0 = *it; // direct conversion, not static_cast<bool>(*it)
			CHECK(bit0);

			++it;
			bool const bit1 = *it;
			CHECK_FALSE(bit1);

			++it;
			bool const bit2 = *it;
			CHECK(bit2);
		}

		SUBCASE("operator+= / operator+ skip ahead by bits") {
			dyn8 b{0b00000000, 0b00000010};

			auto it = b.begin();
			it += 9; // segment 1, offset 1
			CHECK(*it);

			auto const it2 = b.begin() + 9;
			CHECK(*it2);
		}

		SUBCASE("operator-- / operator--(int) walk backwards, wrapping across segments") {
			dyn8 b{0b00000001, 0b10000000};

			auto it = b.begin() + 9; // segment 1, offset 1
			--it;                    // segment 1, offset 0
			CHECK_EQ(it.get(), 0b10000000);

			--it; // wraps back into segment 0, offset 7 (top bit of segment 0)
			CHECK_EQ(it.get(), 0b00000001);
			CHECK_FALSE(*it); // offset 7 of segment 0 is reset

			auto it2 = b.begin() + 3;
			auto const prev = it2--;
			CHECK(prev == b.begin() + 3);
			CHECK(it2 == b.begin() + 2);
		}

		SUBCASE("operator-= / operator- walk backwards by bits") {
			dyn8 b{0b00000000, 0b00000010};

			auto it = b.begin() + 9;
			it -= 9;
			CHECK_FALSE(*it);
			CHECK(it == b.begin());

			auto const it2 = (b.begin() + 9) - 9;
			CHECK(it2 == b.begin());
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
			dyn8 b = zeroed<dyn8>(8);
			CHECK_NOTHROW((dyn8::bit_iterator{b, 7}));
			CHECK_THROWS_AS((dyn8::bit_iterator{b, 8}), std::out_of_range); // dyn8's segment width is 8 bits

			dyn8::bit_iterator it{b, 3};
			CHECK(it == b.begin() + 3);
		}

		SUBCASE("explicit iterator construction with an offset and segment validates both") {
			dyn8 b = zeroed<dyn8>(2 * 8);
			CHECK_NOTHROW((dyn8::bit_iterator{b, 0, 1}));
			CHECK_THROWS_AS((dyn8::bit_iterator{b, 8, 0}), std::out_of_range); // bad offset
			CHECK_THROWS_AS((dyn8::bit_iterator{b, 0, 2}), std::out_of_range); // segment out of bounds (only 2 segments exist)

			dyn8::bit_iterator it{b, 2, 1};
			CHECK(it == b.begin() + 10);
		}

		SUBCASE("reverse iteration (rbegin/rend) visits every bit in reverse order exactly once") {
			dyn8 b{0b00000001, 0b10000000};

			size_t visited = 0;
			int global_ix = 15;
			for (dyn8::reverse_iterator it = b.rbegin(); it != b.rend(); ++it, --global_ix) {
				bool const expected = (global_ix == 0 || global_ix == 15);
				CHECK_EQ(static_cast<bool>(*it), expected);
				++visited;
			}
			CHECK_EQ(visited, b.capacity_in_bits());
			CHECK_EQ(global_ix, -1);
		}

		SUBCASE("reverse iteration on a const bitset") {
			dyn8 const b{0b00000001, 0b10000000};
			size_t visited = 0;
			for (dyn8::const_reverse_iterator it = b.rbegin(); it != b.rend(); ++it) {
				++visited;
			}
			CHECK_EQ(visited, b.capacity_in_bits());
		}
	}

	TEST_CASE("rbegin()/rend() report the correct global index, not just the correct values") {
		// std::reverse_iterator's contract: *rbegin() aliases *(base_end - 1), i.e. the LAST valid
		// element; rend()'s base() is begin() itself. Checked here via reference::ix(), independent
		// of the bit *values*, since a value-only check can't distinguish "correct index" from
		// "coincidentally correct bit pattern read from the wrong index".
		SUBCASE("rbegin() dereferences to index size_in_bits()-1; rend() sits at begin()") {
			dyn8 b{0b00000001, 0b10000000}; // logical size 16
			CHECK_EQ((*b.rbegin()).ix(), 15);
			CHECK_EQ(b.rbegin().base(), b.begin() + b.size_in_bits());
			CHECK_EQ(b.rend().base(), b.begin());
		}

		SUBCASE("walking rbegin()..rend() visits indices in strictly descending order") {
			dyn8 b{0xAA, 0x55, 0x0F}; // logical size 24
			size_t expected_ix = b.size_in_bits() - 1;
			size_t visited = 0;
			for (auto it = b.rbegin(); it != b.rend(); ++it) {
				CHECK_EQ((*it).ix(), expected_ix);
				++visited;
				if (expected_ix > 0) --expected_ix;
			}
			CHECK_EQ(visited, b.size_in_bits());
		}

		SUBCASE("rbegin() + n dereferences to index size_in_bits()-1-n (random-access arithmetic)") {
			dyn8 b{0xAA, 0x55}; // logical size 16
			auto const n_bits = static_cast<std::ptrdiff_t>(b.size_in_bits());
			for (std::ptrdiff_t n = 0; n < n_bits; ++n) {
				CHECK_EQ((*(b.rbegin() + n)).ix(), static_cast<size_t>(n_bits - 1 - n));
			}
		}

		SUBCASE("const bitset: same index contract as the non-const overload") {
			dyn8 const b{0b00000001, 0b10000000};
			CHECK_EQ((*b.rbegin()).ix(), 15);
			CHECK_EQ(b.rbegin().base(), b.begin() + b.size_in_bits());
			CHECK_EQ(b.rend().base(), b.begin());
		}

		SUBCASE("non-segment-aligned logical size: rbegin() still starts at size_in_bits()-1, not capacity-1") {
			dyn8 b{};
			b.set(11);
			b.reset(11); // logical size 12 (not a multiple of the 8-bit segment width)
			REQUIRE_EQ(b.size_in_bits(), 12);
			CHECK_EQ((*b.rbegin()).ix(), 11);
			CHECK_EQ(b.rbegin().base(), b.begin() + 12);
		}
	}

	TEST_CASE("bitset_iterator satisfies std::random_access_iterator") {
		// compile-time only: if this ever regresses, this is where it should surface, rather than
		// as a cryptic constraint-not-satisfied error deep inside <ranges>/<algorithm>.
		static_assert(std::default_initializable<dyn64::bit_iterator>);
		static_assert(std::default_initializable<dyn64::const_bit_iterator>);
		static_assert(std::totally_ordered<dyn64::bit_iterator>);
		static_assert(std::totally_ordered<dyn64::const_bit_iterator>);
		static_assert(std::sized_sentinel_for<dyn64::bit_iterator, dyn64::bit_iterator>);
		static_assert(std::random_access_iterator<dyn64::bit_iterator>);
		static_assert(std::random_access_iterator<dyn64::const_bit_iterator>);
		CHECK(true);
	}

	TEST_CASE("the public iterator aliases name what the accessors return") {
		// none of the public aliases carries a _t suffix - that spelling belongs to the private,
		// mode-parameterized templates they are instantiated from.
		static_assert(std::is_same_v<decltype(std::declval<dyn64 &>().begin()), dyn64::bit_iterator>);
		static_assert(std::is_same_v<decltype(std::declval<dyn64 const &>().begin()), dyn64::const_bit_iterator>);
		static_assert(std::is_same_v<decltype(std::declval<dyn64 &>().rbegin()), dyn64::reverse_iterator>);
		static_assert(std::is_same_v<decltype(std::declval<dyn64 const &>().rbegin()), dyn64::const_reverse_iterator>);
		static_assert(std::is_same_v<decltype(std::declval<dyn64 &>().rend()), dyn64::reverse_iterator>);
		static_assert(std::is_same_v<decltype(std::declval<dyn64 const &>().rend()), dyn64::const_reverse_iterator>);
		static_assert(std::is_same_v<decltype(std::declval<dyn64 &>().positions_begin()), dyn64::positional_iterator>);
		static_assert(std::is_same_v<decltype(std::declval<dyn64 const &>().positions_begin()), dyn64::const_positional_iterator>);

		// and they are the reverse adaptors over the bit iterators, not some other type
		static_assert(std::is_same_v<dyn64::reverse_iterator, std::reverse_iterator<dyn64::bit_iterator>>);
		static_assert(std::is_same_v<dyn64::const_reverse_iterator, std::reverse_iterator<dyn64::const_bit_iterator>>);
		static_assert(std::is_same_v<dyn64::reverse_iterator::value_type, bool>);
		static_assert(std::is_same_v<dyn64::value_type, bool>);
		static_assert(std::is_same_v<dyn64::reference, dyn64::bit_iterator::reference>);

		static_assert(std::bidirectional_iterator<dyn64::reverse_iterator>);
		static_assert(std::bidirectional_iterator<dyn64::const_reverse_iterator>);
		CHECK(true);
	}

	TEST_CASE("bitset_iterator new random-access operations") {
		SUBCASE("operator[] reads the bit at begin() + n without moving the iterator") {
			dyn8 b{0b00000000, 0b00000010};
			auto const it = b.begin();

			CHECK_FALSE(static_cast<bool>(it[0]));
			CHECK(static_cast<bool>(it[9])); // segment 1, offset 1
			CHECK_EQ(it[9].ix(), 9);

			// must not have moved
			CHECK_EQ(it, b.begin());
		}

		SUBCASE("operator[] matches *(it + n) for every position") {
			dyn8 b{0b10110010, 0b01001101};
			auto const it = b.begin();
			for (size_t n = 0; n < b.capacity_in_bits(); ++n) {
				CAPTURE(n);
				CHECK_EQ(static_cast<bool>(it[n]), static_cast<bool>(*(it + n)));
			}
		}

		SUBCASE("relational operators order iterators by their global bit position") {
			dyn8 b = zeroed<dyn8>(3 * 8);
			auto const low = b.begin() + 3;
			auto const mid = b.begin() + 9;   // different segment
			auto const high = b.begin() + 20;

			CHECK(low < mid);
			CHECK(mid < high);
			CHECK(low < high);
			CHECK(mid > low);
			CHECK(low <= low);
			CHECK(low <= mid);
			CHECK(mid >= low);
			CHECK(low >= low);
			CHECK_FALSE(mid < low);
			CHECK_FALSE(high <= mid);

			CHECK_EQ(low <=> low, std::strong_ordering::equal);
			CHECK_EQ(low <=> mid, std::strong_ordering::less);
			CHECK_EQ(high <=> mid, std::strong_ordering::greater);
		}

		SUBCASE("commutative operator+ : n + it == it + n") {
			dyn8 b = zeroed<dyn8>(2 * 8);
			auto const it = b.begin() + 2;

			CHECK((5 + it) == (it + 5));
			CHECK_EQ((5 + it).get(), (it + 5).get());
		}

		SUBCASE("default-constructed iterators are equality-comparable and independent of any bitset") {
			dyn8::bit_iterator a{};
			dyn8::bit_iterator b{};
			CHECK(a == b);

			dyn8 bs = zeroed<dyn8>(8);
			auto valid = bs.begin();
			a = valid;
			CHECK(a == valid);
			CHECK(a != b);
		}

		SUBCASE("negative offsets: it += (-n) matches it -= n, it -= (-n) matches it += n") {
			dyn8 b = zeroed<dyn8>(2 * 8);
			auto const base = b.begin() + 9;

			dyn8::bit_iterator::difference_type const n = 3;

			auto plus_neg = base;
			plus_neg += -n;
			CHECK_EQ(plus_neg, base - n);

			auto minus_neg = base;
			minus_neg -= -n;
			CHECK_EQ(minus_neg, base + n);

			CHECK_EQ(base + (-n), base - n);
			CHECK_EQ(base - (-n), base + n);
		}

		SUBCASE("negative offsets round-trip back to the exact starting position") {
			dyn8 b = zeroed<dyn8>(3 * 8);
			auto const base = b.begin() + 15;
			dyn8::bit_iterator::difference_type const n = 7;

			auto it = base + n;
			it += -n;
			CHECK_EQ(it, base);

			auto it2 = base - n;
			it2 -= -n;
			CHECK_EQ(it2, base);
		}

		SUBCASE("std::next/std::prev/std::distance work via the random-access fast path") {
			dyn8 b = zeroed<dyn8>(3 * 8);
			auto const it = b.begin() + 5;

			auto const next5 = std::next(it, 5);
			CHECK_EQ(next5, b.begin() + 10);

			auto const prev3 = std::prev(it, 3);
			CHECK_EQ(prev3, b.begin() + 2);

			CHECK_EQ(std::distance(prev3, next5), 8);
			CHECK_EQ(std::distance(next5, prev3), -8);
		}

		SUBCASE("std::advance with a negative distance moves backwards correctly") {
			dyn8 b = zeroed<dyn8>(3 * 8);
			auto it = b.begin() + 12;
			std::advance(it, -5);
			CHECK_EQ(it, b.begin() + 7);
		}
	}

	TEST_CASE("shifts") {
		SUBCASE("operator<<= moves a single bit toward higher indices, like x <<= n on an integer") {
			// pins down direction unambiguously with one moving bit: index 0 is the LSB (see the
			// formatter/iterator ordering notes elsewhere), so "left shift" must move it toward
			// higher indices, exactly like multiplying a plain integer by 2^n would.
			dyn8 b{0b00000001}; // bit 0 set
			b <<= 3;
			CHECK_FALSE(b.test(0));
			CHECK_FALSE(b.test(1));
			CHECK_FALSE(b.test(2));
			CHECK(b.test(3)); // moved from index 0 to index 3
			CHECK_EQ(b.count(), 1);
		}

		SUBCASE("operator>>= moves a single bit toward lower indices, like x >>= n on an integer") {
			dyn8 b{0b00001000}; // bit 3 set
			b >>= 3;
			CHECK(b.test(0)); // moved from index 3 to index 0
			CHECK_FALSE(b.test(1));
			CHECK_FALSE(b.test(2));
			CHECK_FALSE(b.test(3));
			CHECK_EQ(b.count(), 1);
		}

		SUBCASE("operator<<= moves bits toward higher indices and clears vacated bits") {
			// matches plain-integer semantics (x <<= n multiplies by 2^n): bit i moves to i+shift,
			// the low `shift` bits are zero-filled, and bits shifted past the top are dropped.
			dyn8 b{0b10110011}; // bit(i): 1,1,0,0,1,1,0,1 for i = 0..7
			b <<= 2;
			bool const expected[8] = {false, false, true, true, false, false, true, true};
			for (size_t i = 0; i < 8; ++i) {
				CHECK_EQ(b.test(i), expected[i]);
			}
		}

		SUBCASE("operator>>= moves bits toward lower indices and clears vacated bits") {
			dyn8 b{0b10110011};
			b >>= 2;
			bool const expected[8] = {false, false, true, true, false, true, false, false};
			for (size_t i = 0; i < 8; ++i) {
				CHECK_EQ(b.test(i), expected[i]);
			}
		}

		SUBCASE("operator<< / operator>> produce a shifted copy, leaving the original untouched") {
			dyn8 b{0b10110011}; // bit(i): 1,1,0,0,1,1,0,1 for i = 0..7
			auto const left = b << 2;
			auto const right = b >> 2;

			bool const expected_left[8] = {false, false, true, true, false, false, true, true};
			bool const expected_right[8] = {false, false, true, true, false, true, false, false};
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

		SUBCASE("shift by exactly capacity_in_bits() clears everything") {
			dyn8 b{0xFF, 0xFF};
			b <<= b.capacity_in_bits();
			CHECK_EQ(b.count(), 0);

			dyn8 b2{0xFF, 0xFF};
			b2 >>= b2.capacity_in_bits();
			CHECK_EQ(b2.count(), 0);
		}

		SUBCASE("shift by more than capacity_in_bits() clears everything (regression: used to hang)") {
			// previously, shifting by more bits than the bitset holds caused the internal
			// iterator arithmetic to compare against the static storage capacity instead of
			// the bitset's actual size, so the move/fill loop in operator<<=/>>= never
			// terminated for bitsets whose size() can be less than their max capacity.
			dyn8 b{0xFF, 0xFF};
			b <<= b.capacity_in_bits() + 5;
			CHECK_EQ(b.count(), 0);

			dyn8 b2{0xFF, 0xFF};
			b2 >>= b2.capacity_in_bits() + 5;
			CHECK_EQ(b2.count(), 0);

			bounded64 b3{~0ull, ~0ull}; // size() == 2 segments, capacity 4 segments
			b3 <<= b3.capacity_in_bits() + 30; // within capacity, beyond current size
			CHECK_EQ(b3.count(), 0);

			bounded64 b4{~0ull, ~0ull};
			b4 <<= bounded64_capacity_bits + 100; // beyond even the max capacity
			CHECK_EQ(b4.count(), 0);
		}

		SUBCASE("shift crossing multiple segment boundaries") {
			// the two high segments have to be part of the logical size - the ctor would trim an
			// all-zero top segment, and <<= never grows the storage it shifts within
			dyn8 left = filled<dyn8>(0xFF, 3 * 8); // bits 0..7 set (bottom segment)
			left <<= 10;                           // pushes the low segment's bits up across a boundary
			CHECK_FALSE(left.test(9));
			CHECK(left.test(10));
			CHECK(left.test(17));
			CHECK_FALSE(left.test(18));

			dyn8 right{0x00, 0x00, 0xFF}; // bits 16..23 set (top segment)
			right >>= 10;                 // pulls the high segment's bits down across a boundary
			CHECK_FALSE(right.test(5));
			CHECK(right.test(6));
			CHECK(right.test(13));
			CHECK_FALSE(right.test(14));
		}
	}

	TEST_CASE("all_set / any_set / none_set on an empty bitset") {
		dyn8 b{};
		REQUIRE_EQ(b.capacity_in_bits(), 0);
		CHECK(b.all_set());   // vacuously true: no bit fails to be set
		CHECK_FALSE(b.any_set());
		CHECK(b.none_set());  // vacuously true: no bit is set
	}

	TEST_CASE("bitwise combination with mismatched sizes throws, leaving the receiver untouched") {
		// operator&=/|=/^= check size_match() first and throw std::logic_error immediately on
		// any mismatch, before touching any segment - unlike e.g. set()/test() which throw
		// std::out_of_range instead, and unlike operator== which just reports false.
		SUBCASE("operator&= with a differently-sized operand") {
			dyn8 a{0xFF, 0xFF};
			dyn8 b{0xFF};
			CHECK_THROWS_AS(a &= b, std::logic_error);
			CHECK_EQ(a.count(), 16); // untouched: the throw happens before any segment is combined
		}

		SUBCASE("operator|= with a differently-sized operand") {
			dyn8 a{0x00, 0x00};
			dyn8 b{0xFF};
			CHECK_THROWS_AS(a |= b, std::logic_error);
			CHECK_EQ(a.count(), 0);
		}

		SUBCASE("operator^= with a differently-sized operand") {
			dyn8 a{0x0F, 0x0F};
			dyn8 b{0xFF};
			CHECK_THROWS_AS(a ^= b, std::logic_error);
			CHECK_EQ(a.count(), 8); // untouched
		}

		SUBCASE("operator&/operator|/operator^ (non-mutating) also throw on size mismatch") {
			dyn8 a{0xFF, 0xFF};
			dyn8 b{0xFF};
			CHECK_THROWS_AS((void) (a & b), std::logic_error);
			CHECK_THROWS_AS((void) (a | b), std::logic_error);
			CHECK_THROWS_AS((void) (a ^ b), std::logic_error);
		}
	}

	TEST_CASE("fixed and bounded capacity - equality, bitwise ops and shifts") {
		using fixed64 = bitset<4 * 64, 4 * 64>; // fixed: pass the same bit count twice

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
			a >>= 1;
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
			CHECK_EQ(ix, bounded64_segment_bits * 2); // first bit of the freshly grown 3rd segment
			REQUIRE_EQ(b.capacity_in_bits(), 3 * bounded64_segment_bits);
			CHECK(b.test(ix));
		}
	}

	TEST_CASE("formatting") {
		// exact content is locked down in "formatter output content (hex and binary, big
		// endian)" below - this is just a smoke test that also prints the result for a human to
		// eyeball. debug mode ('?') and an explicit 'x' spec were removed: hex is now the
		// default (no spec at all), and 'b' is the only settable spec.
		dyn8 b{0b10110011, 0x00};
		MESSAGE("hex (default): ", std::format("{}", b));
		MESSAGE("binary: ", std::format("{:b}", b));
	}

    TEST_CASE("formatting long") {
	    dyn64 b_long{}; // parens - see the "size constructor zero-fills every segment" note above
	    b_long.resize(32 * 64);
	    MESSAGE("hex (default): ", std::format("{}", b_long));
	    MESSAGE("binary: ", std::format("{:b}", b_long));
	}

	TEST_CASE("formatter output content (hex and binary, big endian)") {
		// bit_iterator itself walks LEAST-significant bit first: offset 0 is bit 0 of a segment
		// (segment_set uses "1 << offset", so offset 0 is the LSB), and ++it moves toward higher
		// offsets, i.e. toward the MSB. Segments themselves are stored least-significant first too
		// (segment 0 holds the lowest bits). Printing must NOT follow either raw storage order - a
		// human reads/writes binary and hex MSB-first, both across segments and within one - so the
		// formatter emits the most-significant segment first and, within each segment, its most
		// significant bit first. Mixing the two (segment order kept, only bit order reversed) would
		// read as neither convention.

		SUBCASE("binary mode: bit at offset 0 (first bit the iterator visits) prints LAST") {
			// the iterator's first bit (lowest offset, LSB) must end up as the rightmost/last
			// character - the opposite of iteration order.
			dyn8 b{0b00000001};
			CHECK_EQ(std::format("{:b}", b), "[\n[00000001]\n]\n");
		}

		SUBCASE("binary mode: bit at offset 7 (last bit the iterator visits) prints FIRST") {
			// the iterator's last bit (highest offset, MSB) must end up as the leftmost/first
			// character.
			dyn8 b{0b10000000};
			CHECK_EQ(std::format("{:b}", b), "[\n[10000000]\n]\n");
		}

		SUBCASE("binary mode reproduces a mixed bit pattern exactly as written (MSB-first)") {
			// 0b10110011 is itself written MSB-first in the source; if the formatter forgot to
			// reverse the LSB-first iteration order, this would come out as "11001101" instead
			// (the bit-reversal of the actual pattern).
			dyn8 b{0b10110011};
			CHECK_EQ(std::format("{:b}", b), "[\n[10110011]\n]\n");
		}

		SUBCASE("binary mode reverses both segment order and bit order within each segment") {
			// segment 1 (0xFF) is the most significant, so it prints first, still MSB-first
			// internally; segment 0 (0x00) - the least significant - prints last.
			dyn8 b{0x00, 0xFF};
			CHECK_EQ(std::format("{:b}", b), "[\n[11111111]\n[00000000]\n]\n");
		}

		SUBCASE("hex mode renders one segment per line, most-significant segment first") {
			// hex mode reads the raw segment value via it.get() and lets std::format's own hex
			// notation render it - it does not iterate bit-by-bit, so there is no LSB/MSB bit
			// traversal to reverse here. The MSB-first digit order (0xa5, not 0x5a) simply comes
			// from std::format's normal hex formatting of an integer. Segment 1 (0x34) is still the
			// most significant, so it is rendered on the first line.
			dyn8 b{0x12, 0x34};
			CHECK_EQ(std::format("{}", b), "[\n[0x34]\n[0x12]\n]\n");
		}

		SUBCASE("hex mode zero-pads a fully-zero segment to the full width") {
			dyn8 b{0x00, 0xFF};
			CHECK_EQ(std::format("{}", b), "[\n[0xff]\n[0x00]\n]\n");
		}

		SUBCASE("empty bitset formats identically regardless of mode - no segment lines") {
			dyn8 b{};
			CHECK_EQ(std::format("{}", b), "[\n]\n");
			CHECK_EQ(std::format("{:b}", b), "[\n]\n");
		}
	}

	TEST_CASE("formatting edge cases") {
		SUBCASE("no format spec defaults to hex rendering") {
			dyn8 b{0b10110011};
			std::string s;
			CHECK_NOTHROW(s = std::format("{}", b));
		}

		SUBCASE("an unrecognized format spec character throws format_error") {
			// the spec is validated at compile time for a literal format string, so a runtime
			// format string is used here to actually exercise the throwing parse() path.
			dyn8 b{0x00};
			std::string s;
			CHECK_THROWS_AS(s = std::vformat("{:z}", std::make_format_args(b)), std::format_error);
		}

		SUBCASE("debug spec ('?') was removed and is now just as invalid as any other character") {
			// same reasoning as above: a compile-time literal "{:?}" would fail to compile
			// (consteval parse() validation), so a runtime format string is used to exercise
			// the throwing path instead.
			dyn8 b{0x00};
			std::string s;
			CHECK_THROWS_AS(s = std::vformat("{:?}", std::make_format_args(b)), std::format_error);
		}
	}

	TEST_CASE("formatter binary output matches std::bitset<segment_width> per segment") {
		// independent cross-check of the formatter's binary mode against the standard library's
		// own (well-known-correct) bit-to-string conversion, rather than only hand-written literal
		// strings: std::bitset<N>::to_string() is MSB-first, exactly matching what bitset<>'s own
		// binary formatter documents (see "formatter output content" above), so segment-by-segment
		// comparison is a direct, meaningful check rather than a coincidence of two independent
		// hard-coded expectations.
		SUBCASE("single 8-bit segment, several bit patterns") {
			for (uint8_t const pattern : {uint8_t{0x00}, uint8_t{0xFF}, uint8_t{0b10110011},
			                               uint8_t{0b00000001}, uint8_t{0b10000000}, uint8_t{0b01010101}}) {
				dyn8 const b = filled<dyn8>(pattern, 8);
				std::string const expected = "[\n[" + std::bitset<8>(pattern).to_string() + "]\n]\n";
				CAPTURE(static_cast<unsigned>(pattern));
				CHECK_EQ(std::format("{:b}", b), expected);
			}
		}

		SUBCASE("multiple 8-bit segments - each reversed independently, most significant segment first") {
			dyn8 b{0b10110011, 0b00001111, 0b11100001};
			std::string const expected = "[\n["
			                            + std::bitset<8>(0b11100001).to_string() + "]\n["
			                            + std::bitset<8>(0b00001111).to_string() + "]\n["
			                            + std::bitset<8>(0b10110011).to_string() + "]\n]\n";
			CHECK_EQ(std::format("{:b}", b), expected);
		}

		SUBCASE("64-bit segment patterns") {
			for (uint64_t const pattern : {uint64_t{0}, ~uint64_t{0}, uint64_t{0x0123456789ABCDEFull},
			                                uint64_t{1}, uint64_t{1} << 63}) {
				dyn64 const b = filled<dyn64>(pattern, 64);
				std::string const expected = "[\n[" + std::bitset<64>(pattern).to_string() + "]\n]\n";
				CAPTURE(pattern);
				CHECK_EQ(std::format("{:b}", b), expected);
			}
		}

		SUBCASE("random-ish multi-segment 64-bit pattern") {
			dyn64 b{0xDEADBEEFCAFEBABEull, 0x0123456789ABCDEFull};
			std::string const expected = "[\n["
			                            + std::bitset<64>(0x0123456789ABCDEFull).to_string() + "]\n["
			                            + std::bitset<64>(0xDEADBEEFCAFEBABEull).to_string() + "]\n]\n";
			CHECK_EQ(std::format("{:b}", b), expected);
		}
	}

	TEST_CASE("integral segment width coverage") {
		// verifies storage_word selection (and the derived set/test/count/countr_zero paths)
		// across differently-sized plain integral segment types, not just uint8_t/uint64_t.
		auto const check_widths = []<typename T>() {
			using B = bitset<dynamic_extent, dynamic_extent, T>;
			constexpr size_t segment_bits = sizeof(T) * 8;

			B b{};
			b.set(3);
			b.set(segment_bits + 1); // force growth into a 2nd segment

			CHECK_EQ(b.capacity_in_bits(), 2 * segment_bits);
			CHECK_EQ(b.count(), 2);
			CHECK(b.test(3));
			CHECK(b.test(segment_bits + 1));
			CHECK_EQ(b.countr_zero(), 3);
		};

		SUBCASE("uint16_t segments") {
			check_widths.operator()<std::uint16_t>();
		}

		SUBCASE("uint32_t segments") {
			check_widths.operator()<std::uint32_t>();
		}
	}

	TEST_CASE("set with an explicit high/low state") {
		dyn8 b{0x00};
		b.set(3, true);
		CHECK(b.test(3));
		b.set(3, false);
		CHECK_FALSE(b.test(3));

		b.set(3, true);
		b.set(3, true); // setting an already-set bit high again is a no-op
		CHECK(b.test(3));
	}

	TEST_CASE("set_all / reset_all on a fully static, segment-aligned capacity") {
		using fixed64 = bitset<4 * 64, 4 * 64>; // fixed: pass the same bit count twice
		constexpr size_t fixed64_capacity_bits = 4 * 64;

		SUBCASE("set_all sets every bit") {
			fixed64 b{0, 0, 0, 0};
			b.set_all();
			CHECK(b.all_set());
			CHECK_EQ(b.count(), fixed64_capacity_bits);
		}

		SUBCASE("reset_all clears every bit") {
			fixed64 b{~0ull, ~0ull, ~0ull, ~0ull};
			b.reset_all();
			CHECK(b.none_set());
			CHECK_EQ(b.count(), 0);
		}
	}

	TEST_CASE("set_all / reset_all now work in every capacity mode, including a non-aligned logical size") {
		// set_all()/reset_all() used to require !has_dynamic_extent; that restriction is gone, so
		// this exercises the previously-disallowed dynamic/bounded modes, plus the case that
		// actually stresses the last-segment logic: a logical size that isn't a whole number of
		// segments, where only the low leftover_bits() bits of the last segment may end up set.

		SUBCASE("dynamic, narrow (uint8_t) segments, non-aligned logical size") {
			dyn8 b{};
			b.set(11); // logical size 12: 1 full 8-bit segment + 4 leftover bits
			REQUIRE_EQ(b.size_in_bits(), 12);

			b.set_all();
			CHECK(b.all_set());
			CHECK_EQ(b.count(), 12); // exactly the logical size - no leaked padding bits
			for (size_t i = 0; i < 12; ++i) {
				CHECK(b.test(i));
			}

			b.reset_all();
			CHECK(b.none_set());
			CHECK_EQ(b.count(), 0);
		}

		SUBCASE("dynamic, default (uint64_t) segments, non-aligned logical size spanning multiple segments") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70: 1 full 64-bit segment + 6 leftover bits, content all 0

			b.set_all();
			CHECK(b.all_set());
			CHECK_EQ(b.count(), 70);
			CHECK(b.test(69)); // last legal (leftover) bit
			// dyn64 has no max_bits cap, so going past logical size is just "not set", not an
			// error - unlike the capped/fixed modes below, where fits_in_storage() rejects it.
			CHECK_FALSE(b.test(70));

			b.reset_all();
			CHECK(b.none_set());
			CHECK_EQ(b.count(), 0);
		}

		SUBCASE("bounded (dynamic size, capped capacity), non-aligned logical size") {
			using bounded_odd = bitset<dynamic_extent, 100>;

			bounded_odd b{};
			b.set(69);
			b.reset(69); // logical size 70, capacity rounds up to 128
			REQUIRE_EQ(b.size_in_bits(), 70);

			b.set_all();
			CHECK(b.all_set());
			CHECK_EQ(b.count(), 70);

			b.reset_all();
			CHECK(b.none_set());
			CHECK_EQ(b.count(), 0);
		}

		SUBCASE("fully static capacity, permanently non-aligned logical size") {
			// extent (2 segments = 128 bits of storage) is fixed regardless of max_bits (100) -
			// logical size is always exactly 100 for this type, never a whole number of segments.
			using fixed_odd = bitset<100, 100>;

			fixed_odd b{0, 0};
			REQUIRE_EQ(b.size_in_bits(), 100);

			b.set_all();
			CHECK(b.all_set());
			CHECK_EQ(b.count(), 100);
			CHECK(b.test(99));
			CHECK_THROWS_AS((void)b.test(100), std::out_of_range);

			b.reset_all();
			CHECK(b.none_set());
			CHECK_EQ(b.count(), 0);
		}
	}

	TEST_CASE("shrink_to_fit") {
		SUBCASE("drops a trailing all-zero segment, keeping the last live one") {
			dyn8 b = filled<dyn8>(0x0FFF, 3 * 8);  // bits 0..11 set, segment 2 all zero
			REQUIRE_EQ(b.capacity_in_bits(), 3 * 8);
			b.shrink_to_fit();
			REQUIRE_EQ(b.capacity_in_bits(), 2 * 8); // segment 2 (0x00) dropped, segment 1 (0x0F) is the boundary and is kept
			for (size_t i = 0; i < 8; ++i) CHECK(b.test(i));
			for (size_t i = 8; i < 12; ++i) CHECK(b.test(i));
			for (size_t i = 12; i < 16; ++i) CHECK_FALSE(b.test(i));
		}

		SUBCASE("skips over multiple trailing all-zero segments to find the last non-zero one") {
			// scanning from the end: segments 3, 2, and 1 are all zero and get skipped over;
			// segment 0 (0xAA) is the first non-zero segment found, so storage is truncated
			// right after it, dropping everything past it.
			dyn8 b = filled<dyn8>(0xAA, 4 * 8);
			b.shrink_to_fit();
			REQUIRE_EQ(b.capacity_in_bits(), 1 * 8);
			CHECK_EQ(b.count(), 4);
		}

		SUBCASE("no shrink happens when the last segment is non-zero") {
			dyn8 b{0xFF, 0xFF, 0x01};
			b.shrink_to_fit();
			CHECK_EQ(b.capacity_in_bits(), 3 * 8);
			CHECK_EQ(b.count(), 17);
		}

		SUBCASE("every segment is zero - shrinks capacity away entirely") {
			dyn8 b{}; // 3 zero-initialized segments
			b.resize(3 * 8);
		    b.shrink_to_fit();
			CHECK_EQ(b.capacity_in_bits(), 0);
			CHECK_EQ(b.size_in_bits(), 0);
		}

		SUBCASE("bounded (max-extent) capacity uses resize instead of storage reconstruction") {
			bounded64 b{~0ull, 0x0Full, 0ull, 0ull};
			b.shrink_to_fit();
			REQUIRE_EQ(b.capacity_in_bits(), 2 * bounded64_segment_bits);
			CHECK_EQ(b.count(), 68);
		}

		SUBCASE("bounded capacity: shrinking never grows logical size past max_bits") {
			// regression: shrink_to_fit's new logical size is rounded UP to a whole segment
			// (ptr_dist * segment_size_in_bits) - when the live (kept) segment is the last
			// physical one and max_bits isn't segment-aligned, that rounding used to overshoot
			// max_bits entirely (size_in_bits() silently reporting more than max_bits allows,
			// and test() then throwing for indices size_in_bits() itself claimed were in range).
			using bounded_odd = bitset<dynamic_extent, 100>; // needs 2 segments (128 bits); leftover = 36
			bounded_odd b{};
			b.set(99); // last valid logical bit, lives in the boundary (last physical) segment
			REQUIRE_EQ(b.size_in_bits(), 100);
			REQUIRE_EQ(b.capacity_in_bits(), 128);

			b.shrink_to_fit();
			CHECK_EQ(b.size_in_bits(), 100);      // must stay capped at max_bits, not round up to 128
			CHECK_EQ(b.capacity_in_bits(), 128);  // the live (boundary) segment is still kept
			CHECK(b.test(99));
			CHECK_THROWS_AS((void) b.test(100), std::out_of_range); // still correctly out of range
		}
	}

	TEST_CASE("positions / set_positions / reset_positions") {
		SUBCASE("positions() yields exactly the set bit positions, including multiple bits per segment") {
			dyn64 b{0b10101, 0b1}; // segment 0: bits 0,2,4 set; segment 1: bit 0 set
			std::array<size_t, 4> const expected{0, 2, 4, 64};

			size_t i = 0;
			for (auto pos : b.positions()) {
				REQUIRE_LT(i, expected.size());
				// pos is a plain value_type (size_t) now - it IS the position, not a proxy with
				// .ix()/operator bool() (position_iterator only ever yields set-bit positions,
				// so there's no separate "is it set" state left to query here).
				CHECK_EQ(pos, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}

		SUBCASE("positions() correctly skips across multiple fully-zero segments") {
			dyn64 b{0b1, 0, 0, 0b1}; // segment 0 and segment 3 each have bit 0 set, 1-2 are empty
			std::array<size_t, 2> const expected{0, 3 * 64};

			size_t i = 0;
			for (auto pos : b.positions()) {
				REQUIRE_LT(i, expected.size());
				CHECK_EQ(pos, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}

		SUBCASE("set_positions sets exactly the given bits") {
			dyn64 b{}; // parens - see the "size constructor zero-fills every segment" note above
			b.resize(2 * 64);
		    b.set_positions(std::array{0uz, 5uz, 127uz});
			CHECK(b.test(0));
			CHECK(b.test(5));
			CHECK(b.test(127));
			CHECK_EQ(b.count(), 3);
		}

		SUBCASE("reset_positions clears exactly the given bits") {
			dyn64 b{~0ull, ~0ull};
			b.reset_positions(std::array{0uz, 5uz, 127uz});
			CHECK_FALSE(b.test(0));
			CHECK_FALSE(b.test(5));
			CHECK_FALSE(b.test(127));
			CHECK_EQ(b.count(), 128 - 3);
		}
	}

	TEST_CASE("position_iterator behavior") {
		// positional_iterator/const_positional_iterator are now plain (non-template) aliases -
		// position_iterator itself dropped its bitset_mode parameter and always uses BitMode
		// internally, since positional iteration is only ever meaningful bit-by-bit. Local
		// aliases kept here purely for the shorter names used throughout this TEST_CASE.
		using pos_it = dyn64::positional_iterator;
		using const_pos_it = dyn64::const_positional_iterator;

		SUBCASE("satisfies std::input_iterator and positions() satisfies std::ranges::input_range") {
			// compile-time only: if these ever regress, this is where it should surface, rather
			// than as a cryptic "no viable constructor/deduction guide" deep inside <ranges>.
			static_assert(std::input_iterator<pos_it>);
			static_assert(std::input_iterator<const_pos_it>);
			static_assert(std::sentinel_for<std::default_sentinel_t, pos_it>);
			static_assert(std::sentinel_for<std::default_sentinel_t, const_pos_it>);
			static_assert(std::ranges::input_range<decltype(std::declval<dyn64 const&>().positions())>);
			CHECK(true);
		}

		SUBCASE("is exactly an input_iterator - it does not (over-)satisfy any stronger category") {
			// position_iterator only ever moves forward one step at a time via seek(), has no
			// default constructor (ctor requires a bitset&), and has no operator+=/-=/[] at all -
			// so unlike bitset_iterator, it must fail default_initializable and every iterator
			// category above input_iterator, and must not expose a subscript operator either
			// (operator[] is a random_access_iterator-only requirement, not an input_iterator one).
			static_assert(!std::default_initializable<pos_it>);
			static_assert(!std::default_initializable<const_pos_it>);

			static_assert(!std::forward_iterator<pos_it>);
			static_assert(!std::forward_iterator<const_pos_it>);

			static_assert(!std::bidirectional_iterator<pos_it>);
			static_assert(!std::random_access_iterator<pos_it>);

			static_assert(!has_subscript_operator<pos_it>);
			static_assert(!has_subscript_operator<const_pos_it>);
			CHECK(true);
		}

		SUBCASE("input_iterator semantics: single-pass forward traversal reaches the sentinel") {
			// exercises the actual runtime behavior behind the concept check above: only
			// operator++ (no --, no +=), operator*, and comparison against std::default_sentinel_t.
			static_assert(std::is_same_v<decltype(std::declval<pos_it&>()++), pos_it>);
			static_assert(std::is_same_v<decltype(++std::declval<pos_it&>()), pos_it&>);

			dyn64 b{0b10101, 0b1}; // segment 0: bits 0,2,4 set; segment 1: bit 0 set -> 4 set bits total
			auto it = b.positions_begin(); // already positioned at the first set bit (ix 0)

			size_t steps = 0;
			for (; it != std::default_sentinel; ++it) {
				++steps;
				REQUIRE_LE(steps, 4); // guard against an infinite loop if seek() ever regresses
			}
			CHECK_EQ(steps, 4);
		}

		SUBCASE("empty bitset: positions_begin() equals positions_end() immediately") {
			dyn64 b{};
			CHECK(b.positions_begin() == b.positions_end());
			CHECK(b.positions_end() == b.positions_begin());
		}

		SUBCASE("positions_begin()/positions_end() walked directly (without positions()) on a non-const bitset") {
			dyn64 b{0b10101, 0b1}; // segment 0: bits 0,2,4 set; segment 1: bit 0 set
			std::array<size_t, 4> const expected{0, 2, 4, 64};

			size_t i = 0;
			for (auto it = b.positions_begin(); it != b.positions_end(); ++it) {
				REQUIRE_LT(i, expected.size());
				// *it is a plain value_type (size_t) now - it IS the position directly, not a
				// proxy with .ix()/operator bool().
				CHECK_EQ(*it, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}

		SUBCASE("dereferencing converts directly to size_t - no .ix() call needed") {
			// position_iterator::operator*() returns value_type (size_t) by value directly -
			// a plain assignment, not a call to .ix(), is enough to pull the position back out.
			dyn64 b{0b10101, 0b1}; // segment 0: bits 0,2,4 set; segment 1: bit 0 set
			std::array<size_t, 4> const expected{0, 2, 4, 64};

			size_t i = 0;
			for (auto it = b.positions_begin(); it != b.positions_end(); ++it) {
				REQUIRE_LT(i, expected.size());
				size_t const pos = *it; // direct conversion, not (*it).ix()
				CHECK_EQ(pos, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}

		SUBCASE("positions() elements convert directly to size_t as well") {
			dyn64 b{0b10101, 0b1};
			std::array<size_t, 4> const expected{0, 2, 4, 64};

			size_t i = 0;
			for (auto pos_ref : b.positions()) {
				REQUIRE_LT(i, expected.size());
				size_t const pos = pos_ref; // direct conversion, not pos_ref.ix()
				CHECK_EQ(pos, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}

		SUBCASE("positions_begin()/positions_end() on a const bitset yield a const_positional_iterator") {
			dyn64 const b{0b10101, 0b1};
			static_assert(std::is_same_v<decltype(b.positions_begin()), const_pos_it>);

			std::array<size_t, 4> const expected{0, 2, 4, 64};
			size_t i = 0;
			for (auto it = b.positions_begin(); it != b.positions_end(); ++it) {
				REQUIRE_LT(i, expected.size());
				CHECK_EQ(*it, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}

		SUBCASE("postfix increment returns the pre-increment position; prefix returns *this by reference") {
			dyn64 b{0b101}; // bits 0 and 2 set
			auto it = b.positions_begin();

			auto const before = it++;
			CHECK_EQ(*before, 0);
			CHECK_EQ(*it, 2);

			auto &ref = ++it;
			CHECK_EQ(&ref, &it); // weakly_incrementable requires "{ ++i } -> same_as<I&>"
			CHECK(it == b.positions_end());
		}

		SUBCASE("copies are independent") {
			dyn64 b{0b101}; // bits 0 and 2 set
			auto it1 = b.positions_begin();
			auto it2 = it1;
			++it1;

			CHECK(it1 != it2);
			CHECK_EQ(*it2, 0);
			CHECK_EQ(*it1, 2);

			it2 = it1;
			CHECK(it1 == it2);
		}

		SUBCASE("dereferencing yields a read-only size_t, not a writable proxy") {
			// unlike bit_iterator's reference proxy, position_iterator::operator*() returns
			// value_type (size_t) by value - there is nothing left to assign through.
			static_assert(!dereference_is_assignable<pos_it>);
			static_assert(!dereference_is_assignable<const_pos_it>);

			dyn64 b{0b1};
			auto it = b.positions_begin();
			CHECK_EQ(*it, 0);
		}

		SUBCASE("positions_begin() skips forward when bit 0 of segment 0 is not actually set") {
			dyn64 b{0b100}; // bit 2 set, bit 0 NOT set
			auto it = b.positions_begin();
			CHECK_EQ(*it, 2);
		}

		SUBCASE("multiple jumps within one segment: bits at its beginning, middle and end") {
			dyn64 b{(1ull << 0) | (1ull << 30) | (1ull << 63)};
			std::array<size_t, 3> const expected{0, 30, 63};

			size_t i = 0;
			for (auto pos : b.positions()) {
				REQUIRE_LT(i, expected.size());
				CHECK_EQ(pos, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}

		SUBCASE("multiple jumps across several segments, interleaved with single-segment gaps") {
			dyn64 b{
				(1ull << 0) | (1ull << 30) | (1ull << 63), // segment 0: beginning, middle, end
				0,                                          // segment 1: empty (single-segment gap)
				(1ull << 0) | (1ull << 40),                 // segment 2: beginning, middle
				0,                                          // segment 3: empty
				0,                                          // segment 4: empty (two in a row -> larger jump)
				(1ull << 63),                                // segment 5: only the last bit
			};
			std::array<size_t, 6> const expected{
				0, 30, 63,               // segment 0
				2 * 64 + 0, 2 * 64 + 40, // segment 2, after skipping segment 1
				5 * 64 + 63,             // segment 5, after skipping segments 3 and 4
			};

			size_t i = 0;
			for (auto pos : b.positions()) {
				REQUIRE_LT(i, expected.size());
				CHECK_EQ(pos, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}

		SUBCASE("large jump: several consecutive fully-zero segments between two set bits") {
			dyn64 b{1ull, 0, 0, 0, 0, 0, (1ull << 5)}; // 5 empty segments in a row between the two bits
			std::array<size_t, 2> const expected{0, 6 * 64 + 5};

			size_t i = 0;
			for (auto pos : b.positions()) {
				REQUIRE_LT(i, expected.size());
				CHECK_EQ(pos, expected[i]);
				++i;
			}
			CHECK_EQ(i, expected.size());
		}
	}

	TEST_CASE("bit count that isn't a multiple of the segment width is honored exactly - no rounding") {
		// the 2nd template parameter (max_bits) is now an exact logical bit-count bound - it is
		// NOT rounded up to a whole segment for the purposes of what indices are legal. Physical
		// storage (capacity_in_bits()) still grows in whole-segment increments under the hood, but
		// size_in_bits()/fits_in_storage() only ever honor the exact max_bits value: index max_bits-1
		// is the last legal one, and index max_bits itself is always out of range, even though the
		// physically-allocated segment has room for it.

		SUBCASE("bounded (dynamic size, capped) capacity: uint64_t segments") {
			// 200 bits doesn't divide evenly by 64 -> physically needs ceil(200/64) = 4 segments =
			// 256 bits of capacity, but the logical bound stays exactly 200.
			using bounded_odd = bitset<dynamic_extent, 200>;
			constexpr size_t expected_capacity_bits = 4 * 64;

			bounded_odd b{};
			REQUIRE_EQ(b.size_in_bits(), 0); // nothing used yet - dynamic size starts at 0, not max_bits
			CHECK_NOTHROW(b.set(199)); // last legal index (max_bits - 1)
			CHECK_EQ(b.size_in_bits(), 200); // logical size grew to exactly ix + 1, not rounded
			CHECK_EQ(b.capacity_in_bits(), expected_capacity_bits); // physical storage did round up to whole segments
			CHECK(b.test(199));
			CHECK_THROWS_AS(b.set(200), std::out_of_range); // exactly max_bits is out of range, despite spare room in the 4th segment
		}

		SUBCASE("fully static capacity: uint64_t segments") {
			// extent must equal the segment count derived from bits (4) for the fully-static flex_array
			// specialization to apply; using 200 (not 256) here is what actually exercises the bound.
			using fixed_odd = bitset<200, 200>;
			constexpr size_t expected_capacity_bits = 4 * 64;

			fixed_odd b{0, 0, 0, 0};
			// fully static bitsets have no separate "used so far" tracking - size_in_bits() is
			// simply max_bits itself from construction, unconditionally.
			REQUIRE_EQ(b.size_in_bits(), 200);
			CHECK_EQ(b.capacity_in_bits(), expected_capacity_bits);

			CHECK_NOTHROW(b.set(199));
			CHECK(b.test(199));
			CHECK_THROWS_AS(b.set(200), std::out_of_range); // still out of range despite the 4th segment physically existing
		}

		SUBCASE("bounded capacity: narrow (uint8_t) segments") {
			// 20 bits doesn't divide evenly by 8 -> physically needs ceil(20/8) = 3 segments = 24
			// bits of capacity, but the logical bound stays exactly 20.
			using bounded_odd8 = bitset<dynamic_extent, 20, uint8_t>;
			constexpr size_t expected_capacity_bits = 3 * 8;

			bounded_odd8 b{};
			REQUIRE_EQ(b.size_in_bits(), 0);
			CHECK_NOTHROW(b.set(19));
			CHECK_EQ(b.size_in_bits(), 20);
			CHECK_EQ(b.capacity_in_bits(), expected_capacity_bits);
			CHECK(b.test(19));
			CHECK_THROWS_AS(b.set(20), std::out_of_range);
		}
	}

	TEST_CASE("logical size (size_in_bits()) tracks exact usage, distinct from rounded-up capacity") {
		SUBCASE("a fresh dynamic bitset starts at logical size 0, not at any rounded segment width") {
			dyn8 b{};
			CHECK_EQ(b.size_in_bits(), 0);
			CHECK_EQ(b.capacity_in_bits(), 0);
		}

		SUBCASE("set(ix) grows logical size to exactly ix + 1, never rounded to a segment boundary") {
			dyn8 b{};
			b.set(3);
			CHECK_EQ(b.size_in_bits(), 4); // exactly ix+1, not rounded up to 8
			CHECK_EQ(b.capacity_in_bits(), 8); // capacity still rounds up to a whole segment
		}

		SUBCASE("capacity_in_bits() stays a whole-segment multiple while size_in_bits() does not") {
			dyn8 b{};
			b.set(9); // segment 1, offset 1 -> needs 2 segments physically
			CHECK_EQ(b.size_in_bits(), 10);
			CHECK_EQ(b.capacity_in_bits(), 16);
		}

		SUBCASE("setting an already-covered lower index does not change logical size") {
			dyn8 b{};
			b.set(20);
			auto const size_after_growth = b.size_in_bits();
			b.set(3); // well within [0, 21) already
			CHECK_EQ(b.size_in_bits(), size_after_growth);
			CHECK_EQ(b.size_in_bits(), 21);
		}

		SUBCASE("setting the same highest index again does not change logical size") {
			dyn8 b{};
			b.set(10);
			auto const size_after_first = b.size_in_bits();
			b.set(10);
			CHECK_EQ(b.size_in_bits(), size_after_first);
			CHECK_EQ(b.size_in_bits(), 11);
		}

		SUBCASE("sequential grows each update logical size to the new maximum index reached") {
			dyn64 b{};
			b.set(5);
			CHECK_EQ(b.size_in_bits(), 6);
			b.set(130);
			CHECK_EQ(b.size_in_bits(), 131);
			b.set(20); // lower than current max - no change
			CHECK_EQ(b.size_in_bits(), 131);
			b.set(1000);
			CHECK_EQ(b.size_in_bits(), 1001);
		}
	}

	TEST_CASE("every mutating operation updates logical size correctly") {
		SUBCASE("set(ix) beyond current logical size grows it and sets the bit") {
			dyn8 b{};
			b.set(10);
			CHECK_EQ(b.size_in_bits(), 11);
			CHECK(b.test(10));
		}

		SUBCASE("set(ix, false) beyond current logical size grows logical size even though the bit stays low") {
			// set(ix, false) delegates to reset(ix), which - like set()/flip() - goes through the
			// same growth path; the bit added by growth is already zero-filled, so this just makes
			// the (already-implicitly-0) bit explicitly part of the logical range.
			dyn8 b{};
			b.set(10, false);
			CHECK_EQ(b.size_in_bits(), 11);
			CHECK_FALSE(b.test(10));
		}

		SUBCASE("reset(ix) beyond current logical size is a no-op - unlike set(), it never grows") {
			dyn8 b{};
			b.reset(10);
			CHECK_EQ(b.size_in_bits(), 0);
			CHECK_FALSE(b.test(10));
		}

		SUBCASE("flip(ix) beyond current logical size grows, exactly like set() does") {
			// reset() is the odd one out: it has nothing to clear past the logical end, so it stays
			// a no-op there, while set() and flip() both bring the index into range
			dyn8 b{};
			b.flip(10);
			CHECK_EQ(b.size_in_bits(), 11);
			CHECK(b.test(10));  // flipped from the zero it was grown with
		}

		SUBCASE("set_first_free() grows logical size by exactly 1 when every existing bit is full") {
			dyn8 b{}; // 1 segment pre-allocated, all zero, logical size 8
			b.resize(8);
			for (size_t i = 0; i < 8; ++i) b.set(i);
			REQUIRE_EQ(b.size_in_bits(), 8);

			auto const ix = b.set_first_free();
			CHECK_EQ(ix, 8);
			CHECK_EQ(b.size_in_bits(), 9); // grew by exactly the one new bit, not a whole segment
			CHECK(b.test(8));
		}

		SUBCASE("set_first_free() returns a free bit within the current logical range without growing it") {
			dyn8 b{0b11111101}; // bit 1 is the only free bit, logical size already 8
			auto const ix = b.set_first_free();
			CHECK_EQ(ix, 1);
			CHECK_EQ(b.size_in_bits(), 8); // unchanged - the free bit was already within range
		}

		SUBCASE("set_positions() grows logical size to cover the maximum position given, regardless of order") {
			dyn64 b{}; // 1 segment, logical size 64
			b.resize(64);
			b.set_positions(std::array{5uz, 70uz, 3uz}); // 70 is neither first nor last in the list
			CHECK_EQ(b.size_in_bits(), 71);
			CHECK(b.test(5));
			CHECK(b.test(70));
			CHECK(b.test(3));
		}

		SUBCASE("reset_positions() never grows logical size - out-of-range positions are no-ops") {
			dyn64 b{};
			b.resize(64);
			b.reset_positions(std::array{5uz, 70uz});
			CHECK_EQ(b.size_in_bits(), 64);
			CHECK_FALSE(b.test(70));
		}
	}

	TEST_CASE("a fully static bitset's logical size never changes") {
		using fixed64 = bitset<4 * 64, 4 * 64>;

		fixed64 b{0, 0, 0, 0};
		REQUIRE_EQ(b.size_in_bits(), 4 * 64); // fixed from construction - has no dynamic tracking at all

		b.set(5);
		CHECK_EQ(b.size_in_bits(), 4 * 64);
		b.reset(100);
		CHECK_EQ(b.size_in_bits(), 4 * 64);
		b.flip(250);
		CHECK_EQ(b.size_in_bits(), 4 * 64);

		CHECK_EQ(b.size_in_bits(), b.capacity_in_bits()); // static: logical size and capacity always coincide
	}

	TEST_CASE("shrink_to_fit keeps logical size and capacity in lockstep") {
		SUBCASE("dropping a trailing all-zero segment shrinks logical size to the new exact capacity") {
			dyn8 b = filled<dyn8>(0x0FFF, 3 * 8);  // segment 2 is the only all-zero trailing segment
			REQUIRE_EQ(b.size_in_bits(), 24);
			b.shrink_to_fit();
			CHECK_EQ(b.size_in_bits(), 16);  // clamped to what the remaining storage holds
			CHECK_EQ(b.capacity_in_bits(), 16);
		}

		SUBCASE("no shrink happens when the last segment is non-zero - logical size is unchanged") {
			dyn8 b{0xFF, 0xFF};
			auto const size_before = b.size_in_bits();
			b.shrink_to_fit();
			CHECK_EQ(b.size_in_bits(), size_before);
			CHECK_EQ(b.size_in_bits(), 16);
			CHECK_EQ(b.capacity_in_bits(), 16);
		}

		SUBCASE("shrink_to_fit on an empty bitset is a safe no-op") {
			dyn8 b{};
			CHECK_NOTHROW(b.shrink_to_fit());
			CHECK_EQ(b.size_in_bits(), 0);
			CHECK_EQ(b.capacity_in_bits(), 0);
		}
	}

	TEST_CASE("logical size is preserved correctly across copy/move") {
		SUBCASE("copy construction preserves logical size, and the copy is independent going forward") {
			dyn8 a{};
			a.set(20);
			REQUIRE_EQ(a.size_in_bits(), 21);

			dyn8 b{a};
			CHECK_EQ(b.size_in_bits(), 21);
			CHECK(b.test(20));

			b.set(50); // grow only the copy
			CHECK_EQ(b.size_in_bits(), 51);
			CHECK_EQ(a.size_in_bits(), 21); // original untouched
		}

		SUBCASE("copy assignment overwrites logical size with the source's") {
			dyn8 a{};
			a.set(20);
			dyn8 b{};
			b.set(3);
			b = a;
			CHECK_EQ(b.size_in_bits(), 21);
			CHECK(b.test(20));
		}

		SUBCASE("move construction preserves logical size") {
			dyn8 a{};
			a.set(20);
			dyn8 b{std::move(a)};
			CHECK_EQ(b.size_in_bits(), 21);
			CHECK(b.test(20));
		}

		SUBCASE("move assignment preserves logical size") {
			dyn8 a{};
			a.set(20);
			dyn8 b{};
			b = std::move(a);
			CHECK_EQ(b.size_in_bits(), 21);
			CHECK(b.test(20));
		}
	}

	TEST_CASE("logical size participates in equality, not just physical segment content") {
		// two bitsets can share the same physical capacity (same number of allocated segments,
		// all-zero content) while having different logical sizes - equality must still tell them
		// apart, rather than only comparing raw segment words.
		dyn8 a{};
		a.set(5);
		a.reset(5); // segment content back to all-zero, but logical size stayed at 6
		REQUIRE_EQ(a.size_in_bits(), 6);
		REQUIRE_EQ(a.capacity_in_bits(), 8);
		REQUIRE_EQ(a.count(), 0);

		dyn8 b{};
		b.set(2);
		b.reset(2); // all-zero content, logical size 3, same 8-bit physical capacity as a
		REQUIRE_EQ(b.size_in_bits(), 3);
		REQUIRE_EQ(b.capacity_in_bits(), 8);
		REQUIRE_EQ(b.count(), 0);

		CHECK_FALSE(a == b); // same capacity and all-zero content, but different logical size
	}

	TEST_CASE("logical size bounds iteration and aggregate queries, not physical capacity") {
		SUBCASE("bit-mode iteration visits exactly size_in_bits() elements") {
			dyn8 b{};
			b.set(9); // logical size 10, capacity 16 (2 segments)
			size_t visited = 0;
			for (auto it = b.begin(); it != b.end(); ++it) ++visited;
			CHECK_EQ(visited, b.size_in_bits());
			CHECK_EQ(visited, 10);
		}

		SUBCASE("physical capacity grows by a whole (hidden) segment, not just up to the requested bit") {
			dyn8 b{};
			b.set(9); // 2 physical segments (segment width is an implementation detail), logical size only reaches partway into the 2nd
			CHECK_EQ(b.capacity_in_bits(), 16);
		}

		SUBCASE("count()/all_set()/any_set()/none_set() only consider bits within logical size") {
			dyn8 b{}; // 1 full segment, 8 logical bits
			b.resize(8);
			for (size_t i = 0; i < 8; ++i) b.set(i);
			b.set(10); // grows into segment 1; bits 8,9 stay 0, bit 10 is set - segment 1 isn't fully set

			CHECK_EQ(b.count(), 9); // 8 (segment 0) + 1 (bit 10) - not segment 1's unused zero bits
			CHECK_FALSE(b.all_set()); // segment 1 isn't fully set, so overall not all-set
			CHECK(b.any_set());
			CHECK_FALSE(b.none_set());
		}
	}

	TEST_CASE("bitwise operators combine bitsets with equal logical size regardless of construction path") {
		// a grown via set(), b via the size constructor - different histories, same logical size.
		dyn8 a{};
		a.set(15);
		REQUIRE_EQ(a.size_in_bits(), 16);

		dyn8 b{};
		b.resize(16);
		b.set(0);
		REQUIRE_EQ(b.size_in_bits(), 16);

		auto const conjunction = a & b;
		CHECK_EQ(conjunction.count(), 0); // a has only bit 15 set, b has only bit 0 set - no overlap
		CHECK_EQ(conjunction.size_in_bits(), 16);

		auto const disjunction = a | b;
		CHECK_EQ(disjunction.count(), 2);
		CHECK_EQ(disjunction.size_in_bits(), 16);
		CHECK(disjunction.test(0));
		CHECK(disjunction.test(15));
	}

	TEST_CASE("countr_zero honors an exact (non-segment-aligned) logical size") {
		// segment width for the default (uint64_t) instantiations used below; bitset<> keeps this
		// private, so - like bounded64_segment_bits above - it is hardcoded here from the known
		// template argument.
		constexpr size_t seg64 = 64;

		SUBCASE("aligned: the set bit sits in an early segment (non-edge - the leftover segment does not exist)") {
			dyn64 b = zeroed<dyn64>(2 * seg64); // 2 full segments (128 bits, aligned)
			b.set(5);                           // bit 5 is the first 1
			CHECK_EQ(b.countr_zero(), 5);
		}

		SUBCASE("aligned: an entirely-zero bitset returns exactly the (segment-aligned) logical size") {
			dyn64 b = zeroed<dyn64>(2 * seg64);
			CHECK_EQ(b.countr_zero(), 2 * seg64);
		}

		SUBCASE("misaligned: the set bit sits in a full segment before the leftover one (non-edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70 (1 full segment + 6 leftover bits), content back to 0
			b.set(3);    // the only 1 bit is in segment 0, well before the leftover segment
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countr_zero(), 3);
		}

		SUBCASE("misaligned: the set bit sits inside the leftover region itself (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70, leftover = 6 valid bits (global 64..69)
			b.set(67);   // offset 3 within the leftover segment
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countr_zero(), seg64 + 3);
		}

		SUBCASE("misaligned: everything is zero, including the leftover - capped at the logical size, not the segment width (edge)") {
			// distinguishes a correct implementation from one that treats the padding bits above
			// logical size as real zeros (over-counts up to a full extra segment width) or one
			// that mishandles the leftover scan and stops too early.
			dyn64 b{};
			b.set(69);
			b.reset(69);
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countr_zero(), 70); // not 128 (2 * seg64)
		}

		SUBCASE("misaligned, narrow (uint8_t) segments") {
			dyn8 b{};
			b.set(11);
			b.reset(11); // logical size 12 (1 full 8-bit segment + 4 leftover bits), all zero
			REQUIRE_EQ(b.size_in_bits(), 12);
			CHECK_EQ(b.countr_zero(), 12);
		}

		SUBCASE("misaligned, bounded (capped-dynamic) capacity") {
			using bounded_odd = bitset<dynamic_extent, 100>;
			bounded_odd b{};
			b.set(69);
			b.reset(69);
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countr_zero(), 70);
		}

		SUBCASE("misaligned, fully-static (fixed) capacity") {
			// extent (2 segments = 128 bits of storage) is fixed regardless of max_bits (100) -
			// logical size is always exactly 100, permanently non-segment-aligned.
			using fixed_odd = bitset<100, 100>;
			fixed_odd b{0, 1ull << 26}; // offset 26 within the 36-bit leftover (global bit 90)
			REQUIRE_EQ(b.size_in_bits(), 100);
			CHECK_EQ(b.countr_zero(), seg64 + 26);
		}
	}

	TEST_CASE("countl_zero honors an exact (non-segment-aligned) logical size") {
		constexpr size_t seg64 = 64;

		SUBCASE("aligned: the set bit sits in the last segment (non-edge - resolved without any fallback)") {
			dyn64 b = zeroed<dyn64>(2 * seg64); // 2 full segments (128 bits, aligned)
			b.set(seg64 + 58);
			CHECK_EQ(b.countl_zero(), 5); // 63 - 58
		}

		SUBCASE("aligned: an entirely-zero bitset returns exactly the (segment-aligned) logical size") {
			dyn64 b = zeroed<dyn64>(2 * seg64);
			CHECK_EQ(b.countl_zero(), 2 * seg64);
		}

		SUBCASE("misaligned: resolved within the leftover region, not at its very top (edge, no fallback)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70, leftover = 6 valid bits (global 64..69)
			b.set(67);   // offset 3 of 0..5 - two leading zero bits (offsets 5, 4) above it
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countl_zero(), 2);
		}

		SUBCASE("misaligned: the leftover is entirely zero, so it falls back to an earlier full segment (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70, leftover all zero
			b.set(5);    // segment 0 (a full, non-last segment) decides the answer
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countl_zero(), 6 + (seg64 - 1 - 5)); // 6 leftover zeros + segment 0's own leading zeros
		}

		SUBCASE("misaligned: everything is zero - capped at the logical size, not the segment width (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69);
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countl_zero(), 70); // not 128
		}

		SUBCASE("misaligned, narrow (uint8_t) segments") {
			// this is the exact shape that used to fail to compile: countl_zero() on a non-aligned
			// bitset with a segment type narrower than int (integer promotion made the masked
			// operand's type disagree with what std::countl_zero requires).
			dyn8 b{};
			b.set(11);
			b.reset(11); // logical size 12, leftover = 4 valid bits (global 8..11)
			b.set(9);    // offset 1 of 0..3 - two leading zero bits (offsets 3, 2) above it
			REQUIRE_EQ(b.size_in_bits(), 12);
			CHECK_EQ(b.countl_zero(), 2);
		}

		SUBCASE("misaligned, bounded (capped-dynamic) capacity") {
			using bounded_odd = bitset<dynamic_extent, 100>;
			bounded_odd b{};
			b.set(69);
			b.reset(69);
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countl_zero(), 70);
		}

		SUBCASE("misaligned, fully-static (fixed) capacity, falls back into an earlier full segment") {
			using fixed_odd = bitset<100, 100>;
			fixed_odd b{1ull << 10, 0}; // leftover (segment 1) all zero; segment 0 decides the answer
			REQUIRE_EQ(b.size_in_bits(), 100);
			CHECK_EQ(b.countl_zero(), 36 + (seg64 - 1 - 10)); // 36 leftover zeros + segment 0's leading zeros
		}
	}

	TEST_CASE("countr_one honors an exact (non-segment-aligned) logical size") {
		// mirrors "countr_zero honors an exact (non-segment-aligned) logical size" with polarity
		// flipped: every case starts from all-ones (via set_all(), which respects logical size) and
		// resets a single bit to stand in for the "boundary" that stops the run.
		constexpr size_t seg64 = 64;

		SUBCASE("aligned: the reset bit sits in an early segment (non-edge - the leftover segment does not exist)") {
			dyn64 b{~(1ull << 5), ~0ull}; // 2 full segments (128 bits, aligned); bit 5 is the first 0
			CHECK_EQ(b.countr_one(), 5);
		}

		SUBCASE("aligned: an entirely-one bitset returns exactly the (segment-aligned) logical size") {
			dyn64 b{~0ull, ~0ull};
			CHECK_EQ(b.countr_one(), 2 * seg64);
		}

		SUBCASE("misaligned: the reset bit sits in a full segment before the leftover one (non-edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70 (1 full segment + 6 leftover bits)
			b.set_all();
			b.reset(3); // the only 0 bit is in segment 0, well before the leftover segment
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countr_one(), 3);
		}

		SUBCASE("misaligned: the reset bit sits inside the leftover region itself (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70, leftover = 6 valid bits (global 64..69)
			b.set_all();
			b.reset(67); // offset 3 within the leftover segment
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countr_one(), seg64 + 3);
		}

		SUBCASE("misaligned: everything is one, including the leftover - capped at the logical size, not the segment width (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69);
			b.set_all();
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countr_one(), 70); // not 128 (2 * seg64)
		}

		SUBCASE("misaligned, narrow (uint8_t) segments") {
			dyn8 b{};
			b.set(11);
			b.reset(11); // logical size 12 (1 full 8-bit segment + 4 leftover bits)
			b.set_all();
			REQUIRE_EQ(b.size_in_bits(), 12);
			CHECK_EQ(b.countr_one(), 12);
		}

		SUBCASE("misaligned, bounded (capped-dynamic) capacity") {
			using bounded_odd = bitset<dynamic_extent, 100>;
			bounded_odd b{};
			b.set(69);
			b.reset(69);
			b.set_all();
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countr_one(), 70);
		}

		SUBCASE("misaligned, fully-static (fixed) capacity") {
			using fixed_odd = bitset<100, 100>;
			fixed_odd b{~0ull, ((1ull << 36) - 1) & ~(1ull << 26)}; // offset 26 within the 36-bit leftover reset
			REQUIRE_EQ(b.size_in_bits(), 100);
			CHECK_EQ(b.countr_one(), seg64 + 26);
		}
	}

	TEST_CASE("countl_one honors an exact (non-segment-aligned) logical size") {
		// mirrors "countl_zero honors an exact (non-segment-aligned) logical size" with polarity
		// flipped. This is exactly the coverage gap that let countl_one() silently return 0 for
		// every non-aligned bitset: it used to fold in the wrong (unmasked) segment range and, even
		// after that was fixed, still used the LSB-scan shape (reduce full segments first, boundary
		// last) instead of the MSB-scan shape countl_zero() already used (boundary first, short-
		// circuiting before ever looking at the full segments).
		constexpr size_t seg64 = 64;

		SUBCASE("aligned: the reset bit sits in the last segment (non-edge - resolved without any fallback)") {
			dyn64 b{~0ull, ~(1ull << 58)}; // 2 full segments (128 bits, aligned)
			CHECK_EQ(b.countl_one(), 5); // 63 - 58
		}

		SUBCASE("aligned: an entirely-one bitset returns exactly the (segment-aligned) logical size") {
			dyn64 b{~0ull, ~0ull};
			CHECK_EQ(b.countl_one(), 2 * seg64);
		}

		SUBCASE("misaligned: resolved within the leftover region, not at its very top (edge, no fallback)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70, leftover = 6 valid bits (global 64..69)
			for (size_t i = 64; i < 70; ++i) b.set(i); // leftover entirely one
			b.reset(67); // offset 3 of 0..5 - two leading ones (offsets 5, 4) above it
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countl_one(), 2);
		}

		SUBCASE("misaligned: the leftover is entirely one, so it falls back to an earlier full segment (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70
			b.set_all(); // leftover entirely one, plus segment 0 entirely one for now
			b.reset(5);  // segment 0 (a full, non-last segment) decides the answer
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countl_one(), 6 + (seg64 - 1 - 5)); // 6 leftover ones + segment 0's own leading ones
		}

		SUBCASE("misaligned: everything is one - capped at the logical size, not the segment width (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69);
			b.set_all();
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countl_one(), 70); // not 128
		}

		SUBCASE("misaligned, narrow (uint8_t) segments") {
			dyn8 b{};
			b.set(11);
			b.reset(11); // logical size 12, leftover = 4 valid bits (global 8..11)
			for (size_t i = 8; i < 12; ++i) b.set(i); // leftover entirely one
			b.reset(9); // offset 1 of 0..3 - two leading ones (offsets 3, 2) above it
			REQUIRE_EQ(b.size_in_bits(), 12);
			CHECK_EQ(b.countl_one(), 2);
		}

		SUBCASE("misaligned, bounded (capped-dynamic) capacity") {
			using bounded_odd = bitset<dynamic_extent, 100>;
			bounded_odd b{};
			b.set(69);
			b.reset(69);
			b.set_all();
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_EQ(b.countl_one(), 70);
		}

		SUBCASE("misaligned, fully-static (fixed) capacity, falls back into an earlier full segment") {
			using fixed_odd = bitset<100, 100>;
			fixed_odd b{~(1ull << 10), (1ull << 36) - 1}; // leftover (segment 1) all-one; segment 0 decides the answer
			REQUIRE_EQ(b.size_in_bits(), 100);
			CHECK_EQ(b.countl_one(), 36 + (seg64 - 1 - 10)); // 36 leftover ones + segment 0's own leading ones
		}
	}

	TEST_CASE("all_set with a non-segment-aligned logical size") {
		SUBCASE("misaligned: an earlier full segment isn't fully set - false without the leftover ever mattering (non-edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70, segment 0 not fully set (all zero), leftover also all zero
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_FALSE(b.all_set());
		}

		SUBCASE("misaligned: every full segment is set, but the leftover is only partially set (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69); // logical size 70, leftover = 6 valid bits (global 64..69)
			for (size_t i = 0; i < 64; ++i) b.set(i); // segment 0 fully set
			b.set(64);
			b.set(65); // only 2 of the 6 leftover bits set
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK_FALSE(b.all_set());
		}

		SUBCASE("misaligned: every full segment is set and the leftover is set exactly up to its width, no more (edge)") {
			dyn64 b{};
			b.set(69);
			b.reset(69);
			b.set_all();
			REQUIRE_EQ(b.size_in_bits(), 70);
			CHECK(b.all_set());
			CHECK_EQ(b.count(), 70); // not 128 - confirms the leftover comparison isn't just "segment is non-zero"
		}

		SUBCASE("misaligned, narrow (uint8_t) segments") {
			dyn8 full{};
			full.set(11);
			full.reset(11);
			full.set_all();
			CHECK(full.all_set());

			dyn8 partial{};
			partial.set(11);
			partial.reset(11);
			for (size_t i = 0; i < 8; ++i) partial.set(i); // segment 0 full, leftover untouched
			CHECK_FALSE(partial.all_set());
		}

		SUBCASE("misaligned, bounded (capped-dynamic) capacity") {
			using bounded_odd = bitset<dynamic_extent, 100>;
			bounded_odd b{};
			b.set(69);
			b.reset(69);
			b.set_all();
			CHECK(b.all_set());

			b.reset(64); // clear a single leftover bit
			CHECK_FALSE(b.all_set());
		}

		SUBCASE("misaligned, fully-static (fixed) capacity") {
			using fixed_odd = bitset<100, 100>;
			// leftover is 36 valid bits (global 64..99); mask sets exactly those, nothing above.
			constexpr uint64_t leftover_mask = (uint64_t{1} << 36) - 1;

			fixed_odd full{~0ull, leftover_mask};
			CHECK(full.all_set());

			fixed_odd partial{~0ull, leftover_mask >> 1}; // one leftover bit short
			CHECK_FALSE(partial.all_set());
		}
	}

	TEST_CASE("operator~ preserves the zero-padding invariant across a non-aligned boundary") {
		SUBCASE("aligned: NOT of all-zero is all-one and vice versa, double negation is the identity") {
			dyn64 b = zeroed<dyn64>(2 * 64);
			auto const notb = ~b;
			CHECK(notb.all_set());
			CHECK_EQ(notb.count(), 2 * 64);

			auto const notnotb = ~notb;
			CHECK(notnotb.none_set());
			CHECK(notnotb == b);
		}

		SUBCASE("misaligned: exactly the logical bits flip - no padding bits leak into count()/all_set()") {
			dyn64 b{};
			b.set(69); // logical size 70, only bit 69 set
			REQUIRE_EQ(b.size_in_bits(), 70);

			auto const notb = ~b;
			CHECK_EQ(notb.count(), 69); // 70 logical bits, 1 was set -> 69 should now be set
			CHECK_FALSE(notb.test(69));
			for (size_t i = 0; i < 69; ++i) {
				CHECK(notb.test(i));
			}
			CHECK_FALSE(notb.all_set()); // bit 69 is 0, so not every logical bit is set

			CHECK((~notb) == b); // double negation round-trips exactly
		}

		SUBCASE("misaligned, narrow (uint8_t) segments") {
			dyn8 b{};
			b.set(11); // logical size 12, only bit 11 set
			auto const notb = ~b;
			CHECK_EQ(notb.count(), 11);
			CHECK_FALSE(notb.test(11));
			CHECK((~notb) == b);
		}

		SUBCASE("misaligned, bounded (capped-dynamic) capacity") {
			using bounded_odd = bitset<dynamic_extent, 100>;
			bounded_odd b{};
			b.set(69);
			auto const notb = ~b;
			CHECK_EQ(notb.count(), 69);
			CHECK_EQ(notb.size_in_bits(), 70);
		}

		SUBCASE("misaligned, fully-static (fixed) capacity") {
			using fixed_odd = bitset<100, 100>;
			fixed_odd b{0, 1ull << 10}; // one bit set, deep inside the 36-bit leftover
			auto const notb = ~b;
			CHECK_EQ(notb.count(), 99); // 100 logical bits, 1 was set
			CHECK_FALSE(notb.all_set());
			CHECK((~notb) == b);
		}
	}

	TEST_CASE("bitwise and/or/xor preserve the zero-padding invariant across a non-aligned boundary") {
		// and/or/xor never touch a bit that both operands agree is 0 - so as long as both operands
		// already keep their padding at 0 (which set()/set_all()/the fixed operator~ above all
		// guarantee), these can't introduce a leak on their own. These identities exercise exactly
		// that: mixing in an operator~ result (the operation that used to leak) and checking the
		// combination still respects the exact logical size.
		SUBCASE("misaligned, narrow (uint8_t) segments") {
			dyn8 a{0b01011010};
			a.set(11); // logical size 12, a mixed (non-trivial) pattern in segment 0 plus a leftover bit

			CHECK((a ^ a).none_set());
			CHECK_EQ((a ^ a).count(), 0);

			auto const xor_not = a ^ ~a;
			CHECK(xor_not.all_set());
			CHECK_EQ(xor_not.count(), 12);

			CHECK((a & ~a).none_set());
			CHECK((a | ~a).all_set());
		}

		SUBCASE("misaligned, default (uint64_t) segments spanning multiple segments") {
			dyn64 a{};
			a.set(69);
			a.reset(69);
			a.set(40);
			a.set(3); // mixed pattern: segment 0 has 2 bits set, leftover has none

			CHECK((a ^ a).none_set());

			auto const xor_not = a ^ ~a;
			CHECK(xor_not.all_set());
			CHECK_EQ(xor_not.count(), 70);

			CHECK((a & ~a).none_set());
			CHECK((a | ~a).all_set());
		}
	}
    // the '0'/'1' characters of a "{:b}"-formatted bitset, in output order, framing stripped
    std::string binary_digits(std::string_view const formatted) {
	    std::string out;
	    for (char const c : formatted) {
		    if (c == '0' || c == '1') {
			    out += c;
		    }
	    }
	    return out;
    }

    // the hex digits of a "{}"-formatted bitset, in output order, "0x" prefixes and framing stripped
    std::string hex_digits(std::string_view const formatted) {
	    std::string out;
	    bool in_number = false;
	    for (char const c : formatted) {
		    if (c == 'x') {
			    in_number = true;
		    } else if (c == ']') {
			    in_number = false;
		    } else if (in_number) {
			    out += c;
		    }
	    }
	    return out;
    }

    // requires-expressions need a real template parameter so an ill-formed expression fails by
    // substitution instead of becoming a hard error.
    template<typename I>
    concept deref_is_assignable = requires(I it, bool b) { *it = b; };

    template<typename I>
    concept has_reversed_difference = requires(I it, std::iter_difference_t<I> n) { n - it; };

    using namespace dice::template_library;

    using dyn8 = bitset<dynamic_extent, dynamic_extent, uint8_t>;
    using dyn64 = bitset<dynamic_extent, dynamic_extent>;
    using capped8 = bitset<dynamic_extent, 10, uint8_t>;  // 10 logical bits, 2 uint8_t segments
    using fixed8 = bitset<10, 10, uint8_t>;               // same, but fully static

    TEST_CASE("default-initialized fixed bitset") {
	    // r3892361831: the fully static storage has no initializer, so `fixed8 b;` adopts whatever
	    // was in that memory. Dirty a buffer and default-initialize into it to make that visible.
	    alignas(fixed8) std::array<std::byte, sizeof(fixed8)> buf{};
	    auto *dirty = reinterpret_cast<volatile std::byte *>(buf.data());
	    for (size_t i = 0; i < buf.size(); ++i) {
		    dirty[i] = std::byte{0xff};  // volatile, so the fill cannot be optimized away
	    }

	    auto const *b = new (static_cast<void *>(buf.data())) fixed8;  // default-init, not `fixed8{}`
	    CHECK_EQ(b->count(), 0);
	    CHECK_FALSE(b->any_set());
	    CHECK(b->none_set());

	    // value-initialization is only saved by zero-init happening before the defaulted ctor runs
	    fixed8 const value_initialized{};
	    CHECK_EQ(value_initialized.count(), 0);
    }

    TEST_CASE("initializer list can exceed max_bits") {
	    // r3892379162: bits_ is set from the list width without consulting max_bits, so the bitset
	    // claims 16 logical bits while refusing every index >= 10.
	    CHECK_THROWS_AS((capped8{0xff, 0xff}), std::length_error);
    }

    TEST_CASE("shrink_to_fit grows the logical size") {
	    // r3892398232: bits_ is recomputed as a whole number of segments, which rounds up.
	    SUBCASE("uncapped") {
		    dyn64 b{};
		    b.set(3);
		    REQUIRE_EQ(b.size_in_bits(), 4);

		    b.shrink_to_fit();
		    CHECK_EQ(b.size_in_bits(), 4);
		    CHECK_EQ(b.count(), 1);

		    dyn64 unshrunk{};
		    unshrunk.set(3);
		    CHECK(unshrunk == b);  // and the round-up breaks equality
	    }

	    SUBCASE("capped") {
		    bitset<dynamic_extent, 100> b{};
		    b.set(3);
		    b.shrink_to_fit();
		    CHECK_EQ(b.size_in_bits(), 4);
	    }
    }

    TEST_CASE("set/reset/flip disagree about growing") {
	    // r3892413481: set() grows the logical size, reset() and flip() bail out instead. Whichever
	    // convention wins, the three have to agree.
	    dyn64 via_set{};
	    via_set.set(5, false);
	    dyn64 via_reset{};
	    via_reset.reset(5);
	    dyn64 via_flip{};
	    via_flip.flip(5);

	    REQUIRE_EQ(via_set.size_in_bits(), 6);
	    CHECK_EQ(via_flip.size_in_bits(), via_set.size_in_bits());
	    CHECK(via_flip.test(5));  // flip() dropped the bit entirely
    }

    TEST_CASE("out-of-range indices are rejected by every mutator") {
	    // not from the review: index 20 used to be a hard error for half the mutators on a capped
	    // bitset and a silent no-op for the other half. All six agree on set()'s contract now.
	    capped8 b{};
	    b.set(9);
	    REQUIRE_EQ(b.size_in_bits(), 10);  // == max_bits

	    std::vector<size_t> const out_of_range{20};
	    CHECK_THROWS_AS(b.set(20), std::out_of_range);
	    CHECK_THROWS_AS((void)b.test(20), std::out_of_range);
	    CHECK_THROWS_AS(b.set_positions(out_of_range), std::out_of_range);

	    CHECK_THROWS_AS(b.reset(20), std::out_of_range);
	    CHECK_THROWS_AS(b.flip(20), std::out_of_range);
	    CHECK_THROWS_AS(b.reset_positions(out_of_range), std::out_of_range);
    }

    TEST_CASE("right shift no longer pulls padding into the logical range") {
	    // the initializer-list ctor used to round the logical size up to a whole segment, so >>=
	    // shifted those padding bits down into the logical range (16 logical bits for a 10-bit
	    // value, of which 6 were padding). The ctor now sizes the bitset to the exact bit width of
	    // the list, so there is no padding left to pull in and >>= only moves real bits.
	    SUBCASE("capped") {
		    capped8 b{0x00, 0x03};
		    REQUIRE_EQ(b.size_in_bits(), 10);
		    REQUIRE_EQ(b.count(), 2);

		    b >>= 4;
		    CHECK_EQ(b.size_in_bits(), 6);
		    CHECK_EQ(b.count(), 2);
		    CHECK(b.test(4));
		    CHECK(b.test(5));
	    }

	    SUBCASE("uncapped") {
		    dyn8 b{0x00, 0x03};
		    REQUIRE_EQ(b.size_in_bits(), 10);
		    REQUIRE_EQ(b.count(), 2);

		    b >>= 4;
		    CHECK_EQ(b.size_in_bits(), 6);
		    CHECK_EQ(b.count(), 2);
		    CHECK(b.test(4));
		    CHECK(b.test(5));
	    }
    }

    TEST_CASE("left shift keeps the bits it moves past the old logical size") {
	    // not from the review, found by the differential tests below: operator<<= grew the logical
	    // size to bits_ + shift, but masked the top segment with the *old* leftover width first,
	    // which cleared every bit that had just landed in the newly exposed range.
	    SUBCASE("a single bit survives the shift") {
		    dyn8 b{};
		    b.set(9);
		    REQUIRE_EQ(b.size_in_bits(), 10);
		    REQUIRE_EQ(b.capacity_in_bits(), 16);

		    b <<= 1;
		    CHECK_EQ(b.size_in_bits(), 11);
		    CHECK_EQ(b.count(), 1);  // the bit moved from 9 to 10, it did not disappear
		    CHECK(b.test(10));
	    }

	    SUBCASE("only the bits shifted past the capacity are lost") {
		    dyn8 b{0xff, 0x03};  // 10 logical bits, all set
		    REQUIRE_EQ(b.count(), 10);

		    b <<= 4;
		    CHECK_EQ(b.size_in_bits(), 14);
		    CHECK_EQ(b.count(), 10);  // bits 4..13, all still inside the 16-bit capacity
	    }
    }

    TEST_CASE("operations follow the logical size, not the storage size") {
	    // not from the review, found by the differential tests below: full_segments_or() used to put
	    // the boundary at the last *storage* segment. operator>>=, resize() and shrink_to_fit() all
	    // leave the storage holding more segments than the logical size spans, so that assumption
	    // was wrong and everything built on it worked on the wrong segment.
	    SUBCASE("the segments above the logical size hold nothing") {
		    dyn8 b{};
		    b.set(9);
		    b >>= 4;  // 6 logical bits, still 2 segments of storage
		    REQUIRE_EQ(b.size_in_bits(), 6);
		    REQUIRE_EQ(b.capacity_in_bits(), 16);

		    b.set_all();
		    CHECK_EQ(b.count(), 6);  // only the 6 logical bits exist
		    CHECK(b.all_set());
		    CHECK_EQ(b.countr_one(), 6);
		    CHECK_EQ(b.countl_one(), 6);
		    CHECK_EQ(std::ranges::distance(b.positions()), 6);

		    auto const flipped = ~b;
		    CHECK(flipped.none_set());  // ~ must not hand back the segment above the logical end
		    CHECK_EQ(flipped.size_in_bits(), 6);
	    }

	    SUBCASE("equal-sized bitsets combine even when their storage differs") {
		    dyn8 wide{};
		    wide.set(9);
		    wide >>= 4;         // 6 logical bits across 2 segments
		    dyn8 const narrow{0x21};  // 6 logical bits in 1 segment
		    REQUIRE_EQ(wide.size_in_bits(), narrow.size_in_bits());
		    REQUIRE(wide.capacity_in_bits() > narrow.capacity_in_bits());

		    // the shorter storage has no second segment to read
		    CHECK_EQ((wide | narrow).count(), 2);
		    CHECK_EQ((wide & narrow).count(), 1);
		    CHECK_EQ((wide ^ narrow).count(), 1);
	    }

	    SUBCASE("equality is about the logical bits, whatever storage holds them") {
		    dyn8 wide{};
		    wide.set(9);
		    wide >>= 4;  // bit 5 of 6, in 2 segments
		    dyn8 const narrow{0x20};

		    CHECK(wide == narrow);
		    CHECK(narrow == wide);
		    CHECK_FALSE(narrow == dyn8{0xa0});  // same bit pattern, 8 logical bits
	    }

	    SUBCASE("resize() down drops the bits above the new size") {
		    dyn8 b{0xff};
		    b.resize(3);
		    CHECK_EQ(b.size_in_bits(), 3);
		    CHECK_EQ(b.count(), 3);

		    b.resize(8);  // the bits it dropped do not come back
		    CHECK_EQ(b.count(), 3);
	    }

	    SUBCASE("shrink_to_fit() never leaves the logical size above the storage") {
		    dyn8 b{};
		    b.set(9);
		    b.reset(9);  // logical size stays at 10, but segment 1 is empty again
		    b.set(0);

		    b.shrink_to_fit();
		    CHECK_EQ(b.capacity_in_bits(), 8);
		    CHECK_EQ(b.size_in_bits(), 8);  // clamped - it cannot claim bits the storage dropped
		    CHECK_EQ(b.count(), 1);
	    }

	    SUBCASE("<<= grows the logical size within the cap, not within the capacity") {
		    capped8 b{};
		    b.set(9);  // 10 logical bits == max_bits, but 2 segments == 16 bits of storage
		    REQUIRE_EQ(b.capacity_in_bits(), 16);

		    b <<= 3;
		    CHECK_EQ(b.size_in_bits(), 10);  // max_bits, not the 16 the storage would allow
		    CHECK_EQ(b.count(), 0);          // bit 9 moved to 12, which is past the cap
	    }
    }

    TEST_CASE("initializer list sizes the bitset to its exact bit width") {
	    // the counterpart to the two cases above: what the ctor now does, stated directly. Trailing
	    // all-zero segments are dropped and the top segment contributes only its significant bits,
	    // so the logical size is the width of the value while the storage stays whole segments.
	    SUBCASE("logical size is the bit width of the value, not a whole segment") {
		    dyn8 const b{0x00, 0x03};
		    CHECK_EQ(b.size_in_bits(), 10);      // 8 + 2 significant bits
		    CHECK_EQ(b.capacity_in_bits(), 16);  // storage still rounds up to whole segments
		    CHECK_EQ(b.count(), 2);
		    CHECK(b.test(8));
		    CHECK(b.test(9));
		    CHECK_FALSE(b.test(10));  // past the logical size - no padding bit to observe
	    }

	    SUBCASE("a single significant bit gives a single logical bit") {
		    dyn8 const b{0b00000001};
		    CHECK_EQ(b.size_in_bits(), 1);
		    CHECK_EQ(b.capacity_in_bits(), 8);
		    CHECK_EQ(b.count(), 1);
	    }

	    SUBCASE("trailing all-zero segments are dropped") {
		    dyn8 const b{0xff, 0x00, 0x00};
		    CHECK_EQ(b.size_in_bits(), 8);
		    CHECK_EQ(b.capacity_in_bits(), 8);
		    CHECK_EQ(b.count(), 8);
	    }

	    SUBCASE("an all-zero list is the empty bitset") {
		    dyn8 const b{0x00, 0x00};
		    CHECK_EQ(b.size_in_bits(), 0);
		    CHECK_EQ(b.capacity_in_bits(), 0);
		    CHECK(b.none_set());
	    }

	    SUBCASE("a top segment with its MSB set stays segment-aligned") {
		    dyn8 const b{0x12, 0xb4};
		    CHECK_EQ(b.size_in_bits(), 16);
		    CHECK_EQ(b.count(), 6);  // 0x12 has 2 bits set, 0xb4 has 4
	    }

	    SUBCASE("the exact width is what max_bits is checked against") {
		    // 0x03 in the top segment is 10 bits wide, which still fits a 10-bit cap ...
		    CHECK_NOTHROW((capped8{0x00, 0x03}));
		    CHECK_NOTHROW((fixed8{0x00, 0x03}));
		    // ... while one more significant bit does not
		    CHECK_THROWS_AS((capped8{0x00, 0x07}), std::length_error);
		    CHECK_THROWS_AS((fixed8{0x00, 0x07}), std::length_error);
	    }

	    SUBCASE("a fixed bitset keeps its declared size regardless of the list") {
		    fixed8 const b{0x01};
		    CHECK_EQ(b.size_in_bits(), 10);  // bits == max_bits == 10, not the list's 1
		    CHECK_EQ(b.count(), 1);
	    }
    }

    TEST_CASE("n + it is required, n - it is not") {
	    // r3892417474: the mirrored operator- was removed because n - it is not subtraction. The
	    // mirrored operator+ has to stay - std::random_access_iterator asks for n + i by name, and
	    // bit_iterator advertises random_access_iterator_tag.
	    CHECK_FALSE(has_reversed_difference<dyn64::bit_iterator>);
	    CHECK_FALSE(has_reversed_difference<dyn64::const_bit_iterator>);

	    CHECK(std::random_access_iterator<dyn64::bit_iterator>);
	    CHECK(std::random_access_iterator<dyn64::const_bit_iterator>);

	    dyn8 b{0x12, 0x34};
	    auto const it = b.begin() + 2;
	    CHECK((5 + it) == (it + 5));
	    CHECK_EQ((5 + it).get(), (it + 5).get());
    }

    TEST_CASE("const iterator is writable") {
	    // r3892403144: reference::operator=(bool) is missing `&& !is_const`, so a const_iterator's
	    // proxy passes every writability check (it only fails once instantiated).
	    CHECK(deref_is_assignable<dyn64::bit_iterator>);
	    CHECK_FALSE(deref_is_assignable<dyn64::const_bit_iterator>);

	    CHECK_FALSE(std::indirectly_writable<dyn64::const_bit_iterator, bool>);
	    CHECK_FALSE((std::output_iterator<dyn64::const_bit_iterator, bool>) );

	    // so a const bitset passes as a writable range: std::ranges::fill(b, true) on a const
	    // bitset satisfies every concept and only fails once it is instantiated
	    CHECK_FALSE((std::ranges::output_range<dyn64 const, bool>) );
	    CHECK((std::ranges::output_range<dyn64, bool>) );
    }

    TEST_CASE("operator-= reports stepping past begin() instead of clamping") {
	    // r3892421436: stepping back past index 0 used to land on 0, which silently broke the
	    // (i -= n) += n == i and (i - n) - i == -n axioms. There is no iterator before begin() to
	    // return, so the step is now rejected.
	    dyn64 b{};
	    b.set(70);
	    auto const it = b.begin() + 3;

	    CHECK_THROWS_AS((void) (it - 5), std::out_of_range);

	    auto mut = it;
	    CHECK_THROWS_AS(mut -= 5, std::out_of_range);
	    CHECK(mut == it);  // and the rejected step leaves the iterator where it was

	    // += with a negative delta routes through operator-= and is rejected the same way
	    CHECK_THROWS_AS(mut += -5, std::out_of_range);
	    CHECK(mut == it);

	    // within range the axioms hold
	    auto const before = it - 3;
	    CHECK(before == b.begin());
	    CHECK_EQ(before - it, -3);
	    CHECK((before + 3) == it);
	    CHECK_EQ((*(before + 3)).ix(), 3);

	    // stepping exactly to begin() is in range; one further is not
	    CHECK_NOTHROW((void) (b.begin() - 0));
	    CHECK_THROWS_AS((void) (b.begin() - 1), std::out_of_range);
    }

    TEST_CASE("class documentation examples") {
	    // r3892435572: the doc block's static and fixed examples use 3 segments and index 42, both
	    // of which exceed a 10-bit bitset.
	    SUBCASE("dynamic") {
		    dyn8 b{0x12, 0x13, 0x14};
		    CHECK_NOTHROW(b.set(4000uz));
	    }

	    SUBCASE("static") {
		    CHECK_THROWS_AS((capped8{0x12, 0x13, 0x14}), std::length_error);

		    capped8 b{0x12, 0x02};  // the widest list the type accepts
		    CHECK_NOTHROW(b.set(6uz));
	    }

	    SUBCASE("fixed") {
		    CHECK_THROWS_AS((fixed8{0x12, 0x13, 0x14}), std::length_error);

		    fixed8 b{0x12, 0x02};
		    CHECK_NOTHROW(b.set(6uz));
	    }
    }

    TEST_CASE("formatter") {
	    // r3674844666: tests_bitset.cpp covers the formatter for segment-aligned bitsets and valid
	    // and invalid specs by now. These are the gaps that coverage still leaves.
	    SUBCASE("printing covers exactly the segments the logical bits live in") {
		    // root cause of r3892440968: the loop used to walk bit positions through the proxy and
		    // ran past size_in_bits(). It now walks segments and reads each one directly, so a
		    // logical size that ends mid-segment prints that segment in full and stops there -
		    // two segments here, not one and not three.
		    dyn8 b{};
		    b.set(9);
		    REQUIRE_EQ(b.size_in_bits(), 10);
		    REQUIRE_EQ(b.capacity_in_bits(), 16);

		    CHECK_EQ(std::format("{:b}", b), "[\n[00000010]\n[00000000]\n]\n");
		    CHECK_EQ(binary_digits(std::format("{:b}", b)).size(), 16);
	    }

	    SUBCASE("hex zero padding is two digits short for wide segments") {
		    // the width passed to {:#0{}x} is sizeof(T) * 2 and does not account for the "0x"
		    // prefix. For uint8_t the width never binds (already characterised in tests_bitset.cpp
		    // as "hex mode drops a fully-zero segment's leading zero"), but for uint64_t it does.
		    dyn64 const b{0x12};
		    CHECK_EQ(std::format("{}", b), "[\n[0x0000000000000012]\n]\n");
	    }

	    SUBCASE("a repeated b is rejected like any other bad spec") {
		    // parse()'s second `if (*it == 'b')` is unreachable, so duplicates slip through
		    dyn8 b{0x12};
		    std::string s;
		    CHECK_THROWS_AS(s = std::vformat("{:bb}", std::make_format_args(b)), std::format_error);
	    }
    }

    TEST_CASE("formatter prints the most significant segment first, MSB-first within a segment") {
	    // not from the review: the formatter used to print MSB-first *within* a segment but put
	    // segment 0 *first*, so its digits read as neither of the two self-consistent orders. It
	    // now agrees with boost, std::bitset and the bitset's own rbegin(), and each segment is
	    // rendered exactly as it sits in memory rather than truncated to the logical size.
	    SUBCASE("two segments") {
		    dyn8 const b{0x12, 0xb4};          // bits 0..7 = 0x12, bits 8..15 = 0xb4, value 0xb412
		    REQUIRE_EQ(b.size_in_bits(), 16);  // the top segment's MSB is set, so nothing is trimmed

		    boost::dynamic_bitset<uint8_t> const ref(16, 0xb412ul);
		    std::bitset<16> const std_ref{0xb412ull};
		    for (size_t i = 0; i < 16; ++i) {
			    REQUIRE_EQ(b.test(i), ref.test(i));
			    REQUIRE_EQ(b.test(i), std_ref.test(i));
		    }

		    std::string want;
		    boost::to_string(ref, want);
		    REQUIRE_EQ(want, std_ref.to_string());

		    // reverse iteration over the same bitset produces boost's order
		    std::string via_rbegin;
		    for (dyn8::const_reverse_iterator it = b.rbegin(); it != b.rend(); ++it) {
			    via_rbegin += static_cast<bool>(*it) ? '1' : '0';
		    }
		    REQUIRE_EQ(via_rbegin, want);

		    CHECK_EQ(binary_digits(std::format("{:b}", b)), want);
		    CHECK_EQ(hex_digits(std::format("{}", b)), "b412");
	    }

	    SUBCASE("a logical size ending mid-segment still prints whole segments") {
		    // same two segments, but the top one's high bits are zero, so the exact-width ctor
		    // leaves the logical size at 14. The formatter shows the segment as it is in memory,
		    // which is boost's 16-bit string; rbegin() covers the 14 logical bits, i.e. the same
		    // digits minus the leading two.
		    dyn8 const b{0x12, 0x34};
		    REQUIRE_EQ(b.size_in_bits(), 14);

		    boost::dynamic_bitset<uint8_t> const ref(16, 0x3412ul);
		    std::string want;
		    boost::to_string(ref, want);

		    CHECK_EQ(binary_digits(std::format("{:b}", b)), want);
		    CHECK_EQ(hex_digits(std::format("{}", b)), "3412");

		    std::string via_rbegin;
		    for (dyn8::const_reverse_iterator it = b.rbegin(); it != b.rend(); ++it) {
			    via_rbegin += static_cast<bool>(*it) ? '1' : '0';
		    }
		    CHECK_EQ(via_rbegin, want.substr(2));
	    }

	    SUBCASE("three segments pin down the segment order") {
		    dyn8 const b{0x01, 0x02, 0x04};  // bits 0, 9 and 18

		    // built over the whole storage, since that is what the formatter renders
		    std::string want;
		    for (size_t i = b.capacity_in_bits(); i-- > 0;) {
			    want += b.test(i) ? '1' : '0';
		    }
		    REQUIRE_EQ(want, "000001000000001000000001");

		    CHECK_EQ(binary_digits(std::format("{:b}", b)), want);
	    }
    }

    // The remaining three run in a subprocess: the proxy's noexcept accessors call test()/set(),
    // which throw for an index the capped storage rejects, so the throw becomes std::terminate.
    // A catchable failure would let the child return 0 instead.

    TEST_CASE("proxy operator bool throws through noexcept") {
	    // r3892406755
	    auto const res = DICE_SANDBOX {
		    capped8 b{};
		    b.set(9);  // logical size 10, capacity 16
		    auto const it = b.begin() + 12;

		    try {
			    if (static_cast<bool>(*it)) {
				    return 1;
			    }
		    } catch (std::out_of_range const &) {
			    return 0;
		    }
		    return 0;
	    };

	    CHECK_EQ(res, SubProcessResult::ExitSuccess);
    }

    TEST_CASE("proxy assignment throws through noexcept") {
	    // r3892404711
	    auto const res = DICE_SANDBOX {
		    capped8 b{};
		    b.set(9);
		    auto const it = b.begin() + 12;

		    try {
			    *it = true;
		    } catch (std::out_of_range const &) {
			    return 0;
		    }
		    return 0;
	    };

	    CHECK_EQ(res, SubProcessResult::ExitSuccess);
    }

    TEST_CASE("format {:b} does not terminate the process") {
	    // r3892440968: the formatter used to read past the logical end through the bit proxy (see
	    // "formatter" above), and on a capped bitset those indices make test() throw out of a
	    // noexcept proxy - i.e. std::terminate. It reads segments directly now.
	    SUBCASE("capped") {
		    auto const res = DICE_SANDBOX {
			    capped8 b{};
			    b.set(9);

			    try {
				    (void) std::format("{:b}", b);
			    } catch (std::out_of_range const &) {
				    return 0;
			    }
			    return 0;
		    };

		    CHECK_EQ(res, SubProcessResult::ExitSuccess);
	    }

	    SUBCASE("fixed") {
		    auto const res = DICE_SANDBOX {
			    fixed8 b{0x00, 0x00};

			    try {
				    (void) std::format("{:b}", b);
			    } catch (std::out_of_range const &) {
				    return 0;
			    }
			    return 0;
		    };

		    CHECK_EQ(res, SubProcessResult::ExitSuccess);
	    }
    }

	using dyn8 = bitset<dynamic_extent, dynamic_extent, uint8_t>;

	// counts a run of equal bits from one end of the reference, capped at its size
	template<typename Ref>
	size_t run_length(Ref const &ref, bool const want, bool const from_msb) {
		for (size_t i = 0; i < ref.size(); ++i) {
			if (ref.test(from_msb ? ref.size() - 1 - i : i) != want) {
				return i;
			}
		}
		return ref.size();
	}

	template<typename Sut, typename Ref>
	void compare(Sut const &sut, Ref const &ref, size_t const logical) {
		REQUIRE_EQ(sut.size_in_bits(), logical);
		for (size_t i = 0; i < logical; ++i) {
			REQUIRE_EQ(sut.test(i), ref.test(i));
		}
		REQUIRE_EQ(sut.count(), ref.count());
		REQUIRE_EQ(sut.all_set(), ref.all());
		REQUIRE_EQ(sut.any_set(), ref.any());
		REQUIRE_EQ(sut.none_set(), ref.none());

		REQUIRE_EQ(sut.countr_zero(), run_length(ref, false, false));
		REQUIRE_EQ(sut.countl_zero(), run_length(ref, false, true));
		REQUIRE_EQ(sut.countr_one(), run_length(ref, true, false));
		REQUIRE_EQ(sut.countl_one(), run_length(ref, true, true));

		std::vector<size_t> want;
		for (size_t i = 0; i < logical; ++i) {
			if (ref.test(i)) {
				want.push_back(i);
			}
		}
		std::vector<size_t> got;
		for (auto const pos : sut.positions()) {
			got.push_back(pos);
		}
		REQUIRE(got == want);

		// forward and reverse iteration must both cover exactly the logical bits
		std::vector<bool> forward;
		for (bool const b : sut) {
			forward.push_back(b);
		}
		REQUIRE_EQ(forward.size(), logical);
		for (size_t i = 0; i < logical; ++i) {
			REQUIRE_EQ(forward[i], ref.test(i));
		}

		size_t reverse_seen = 0;
		// named rather than auto, so every instantiation has to expose the alias under this name
		for (typename Sut::const_reverse_iterator it = sut.rbegin(); it != sut.rend(); ++it, ++reverse_seen) {
			REQUIRE_EQ(static_cast<bool>(*it), ref.test(logical - 1 - reverse_seen));
		}
		REQUIRE_EQ(reverse_seen, logical);

		REQUIRE(sut.capacity_in_bits() >= sut.size_in_bits());
		REQUIRE(sut.count() <= sut.size_in_bits());
	}

	// flips a random subset of an operand pair in lockstep, so the two stay equal-sized
	template<typename Sut, typename Ref, typename Rng>
	std::pair<Sut, Ref> mixed_operand(Sut const &sut, Ref const &ref, Rng &rng, size_t const logical) {
		auto sut_other = sut;
		auto ref_other = ref;
		for (size_t i = 0; i < logical; ++i) {
			if (rng() % 2 == 0) {
				sut_other.flip(i);
				ref_other.flip(i);
			}
		}
		return {sut_other, ref_other};
	}

	TEST_CASE_TEMPLATE("fully static bitsets match std::bitset", Seg, uint8_t, uint16_t, uint64_t) {
		static constexpr size_t bits = 8 * sizeof(Seg) + 2;  // two segments plus a 2-bit remainder
		using sut_type = bitset<bits, bits, Seg>;

		sut_type sut{};
		std::bitset<bits> ref{};
		std::mt19937 rng{1000u + static_cast<unsigned>(sizeof(Seg))};

		compare(sut, ref, bits);

		for (int step = 0; step < 600; ++step) {
			auto const ix = rng() % bits;
			auto const shift = rng() % (bits + 3);

			switch (rng() % 11) {
				case 0: sut.set(ix), ref.set(ix); break;
				case 1: sut.reset(ix), ref.reset(ix); break;
				case 2: sut.flip(ix), ref.flip(ix); break;
				case 3: sut.set_all(), ref.set(); break;
				case 4: sut.reset_all(), ref.reset(); break;
				case 5: sut <<= shift, ref <<= shift; break;
				case 6: sut >>= shift, ref >>= shift; break;
				case 7: sut = ~sut, ref = ~ref; break;
				default: {
					auto const [sut_other, ref_other] = mixed_operand(sut, ref, rng, bits);
					if (rng() % 3 == 0) {
						sut &= sut_other, ref &= ref_other;
					} else if (rng() % 2 == 0) {
						sut |= sut_other, ref |= ref_other;
					} else {
						sut ^= sut_other, ref ^= ref_other;
					}
					break;
				}
			}

			compare(sut, ref, bits);
		}
	}

	TEST_CASE_TEMPLATE("growable bitsets match boost::dynamic_bitset", Seg, uint8_t, uint16_t, uint64_t) {
		using sut_type = bitset<dynamic_extent, dynamic_extent, Seg>;
		static constexpr size_t seg_bits = 8 * sizeof(Seg);

		sut_type sut{};
		boost::dynamic_bitset<Seg> ref{};
		std::mt19937 rng{2000u + static_cast<unsigned>(sizeof(Seg))};

		compare(sut, ref, 0);

		for (int step = 0; step < 400; ++step) {
			// mix small steps with jumps far past the end, to exercise multi-segment growth
			auto const span = (rng() % 4 == 0) ? ref.size() + 200 : ref.size() + 8;
			auto const ix = rng() % std::max<size_t>(span, 8);
			auto const shift = (rng() % 2 == 0) ? seg_bits * (rng() % 4) : rng() % (3 * ref.size() + 16);

			auto const grow_to = [&ref](size_t const n) {
				if (n >= ref.size()) {
					ref.resize(n + 1, false);
				}
			};

			switch (rng() % 12) {
				case 0: sut.set(ix), grow_to(ix), ref.set(ix); break;
				case 1: sut.set(ix, false), grow_to(ix), ref.reset(ix); break;
				case 2:
					sut.reset(ix);
					if (ix < ref.size()) {
						ref.reset(ix);
					}
					break;
				case 3:
					// flip() grows the logical size exactly like set() does
					sut.flip(ix), grow_to(ix), ref.flip(ix);
					break;
				case 4: sut.set_all(), ref.set(); break;
				case 5: sut.reset_all(), ref.reset(); break;
				case 6: {
					// <<= grows the logical size by the shift, capped at the capacity (the storage
					// never grows), so the reference has to be resized before it shifts
					ref.resize(std::min(ref.size() + shift, sut.capacity_in_bits()), false);
					sut <<= shift, ref <<= shift;
					break;
				}
				case 7:
					// >>= shrinks the logical size by the shift
					sut >>= shift, ref >>= shift;
					ref.resize(shift > ref.size() ? 0 : ref.size() - shift, false);
					break;
				case 8: sut = ~sut, ref = ~ref; break;
				case 9:
				case 10: {
					auto const [sut_other, ref_other] = mixed_operand(sut, ref, rng, ref.size());
					if (rng() % 3 == 0) {
						sut &= sut_other, ref &= ref_other;
					} else if (rng() % 2 == 0) {
						sut |= sut_other, ref |= ref_other;
					} else {
						sut ^= sut_other, ref ^= ref_other;
					}
					break;
				}
				default: {
					// first index that is still low, or one past the end when the set is full
					size_t want = ref.size();
					for (size_t i = 0; i < ref.size(); ++i) {
						if (!ref.test(i)) {
							want = i;
							break;
						}
					}
					if (want == ref.size()) {
						ref.resize(want + 1, false);
					}
					ref.set(want);
					REQUIRE_EQ(sut.set_first_free(), want);
					break;
				}
			}

			compare(sut, ref, ref.size());
		}
	}

	TEST_CASE_TEMPLATE("capped bitsets match boost::dynamic_bitset", Seg, uint8_t, uint64_t) {
		static constexpr size_t max_bits = 8 * sizeof(Seg) + 2;
		using sut_type = bitset<dynamic_extent, max_bits, Seg>;

		sut_type sut{};
		boost::dynamic_bitset<Seg> ref{};
		std::mt19937 rng{3000u + static_cast<unsigned>(sizeof(Seg))};

		compare(sut, ref, 0);

		for (int step = 0; step < 400; ++step) {
			auto const ix = rng() % (max_bits + 4);  // deliberately reaches past the cap
			auto const shift = rng() % (max_bits + 3);

			auto const grow_to = [&ref](size_t const n) {
				if (n >= ref.size()) {
					ref.resize(n + 1, false);
				}
			};

			switch (rng() % 10) {
				case 0:
				case 1:
					// past the cap set() must throw and leave the bitset untouched
					if (ix >= max_bits) {
						CHECK_THROWS_AS(sut.set(ix), std::out_of_range);
					} else {
						sut.set(ix), grow_to(ix), ref.set(ix);
					}
					break;
				case 2:
					// past the cap every mutator throws, exactly like set()
					if (ix >= max_bits) {
						CHECK_THROWS_AS(sut.reset(ix), std::out_of_range);
					} else {
						sut.reset(ix);
						if (ix < ref.size()) {
							ref.reset(ix);
						}
					}
					break;
				case 3:
					if (ix >= max_bits) {
						CHECK_THROWS_AS(sut.flip(ix), std::out_of_range);
					} else {
						sut.flip(ix), grow_to(ix), ref.flip(ix);
					}
					break;
				case 4: sut.set_all(), ref.set(); break;
				case 5: sut.reset_all(), ref.reset(); break;
				case 6: {
					// <<= grows the logical size by the shift, capped at what the bitset can hold -
					// the storage never grows, and it never reaches past max_bits
					ref.resize(std::min({ref.size() + shift, sut.capacity_in_bits(), max_bits}), false);
					sut <<= shift, ref <<= shift;
					break;
				}
				case 7:
					// >>= shrinks the logical size by the shift
					sut >>= shift, ref >>= shift;
					ref.resize(shift > ref.size() ? 0 : ref.size() - shift, false);
					break;
				case 8: sut = ~sut, ref = ~ref; break;
				default: {
					auto const [sut_other, ref_other] = mixed_operand(sut, ref, rng, ref.size());
					sut ^= sut_other, ref ^= ref_other;
					break;
				}
			}

			compare(sut, ref, ref.size());
			REQUIRE(sut.size_in_bits() <= max_bits);
		}
	}

	TEST_CASE("range algorithms work through the bit proxy") {
		SUBCASE("fill / count / equal / find") {
			dyn8 b{};
			b.set(9);  // 10 logical bits
			std::ranges::fill(b, true);
			CHECK_EQ(b.count(), 10);
			CHECK(b.all_set());

			b.reset_all();
			b.set(1);
			b.set(9);
			CHECK_EQ(std::ranges::count(b, true), 2);
			CHECK_EQ((*std::ranges::find(b, true)).ix(), 1);

			std::vector<bool> const want{false, true, false, false, false, false, false, false, false, true};
			CHECK(std::ranges::equal(b, want));
		}

		SUBCASE("copy into the bitset") {
			dyn8 b{};
			b.set(9);
			b.reset(9);

			std::vector<bool> const src{true, false, true, true, false, false, false, false, true, true};
			std::ranges::copy(src, b.begin());
			CHECK(std::ranges::equal(b, src));
			CHECK_EQ(b.count(), 5);
		}

		SUBCASE("reverse and iter_swap permute bits in place") {
			dyn8 b{};
			b.set(9);
			b.set(1);
			b.set(0);

			std::ranges::reverse(b);
			CHECK_EQ(b.count(), 3);
			CHECK(b.test(0));  // was bit 9
			CHECK(b.test(8));  // was bit 1
			CHECK(b.test(9));  // was bit 0

			std::ranges::iter_swap(b.begin(), b.begin() + 1);
			CHECK_FALSE(b.test(0));
			CHECK(b.test(1));
		}

		SUBCASE("proxy assignment copies the value, not the binding") {
			dyn8 b{};
			b.set(9);
			b.set(0);

			*(b.begin() + 1) = *b.begin();
			CHECK(b.test(1));
			CHECK_EQ(b.count(), 3);
		}
	}
}
