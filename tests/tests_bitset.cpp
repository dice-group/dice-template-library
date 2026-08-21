#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include <dice/template-library/bitset.hpp>

#include <array>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

// helper for the position_iterator concept checks below: requires-expressions need a genuine
// template parameter to fail via substitution rather than a hard compile error, so this can't be
// inlined as `requires(dyn64::positional_iterator<...> it, ...) { it[n]; }` inside the static_assert.
template<typename I>
concept has_subscript_operator = requires(I i, std::iter_difference_t<I> n) { i[n]; };

// position_iterator's dereference is a plain value_type (size_t) now, not a writable proxy -
// verifies there is nothing to assign through it.
template<typename I>
concept dereference_is_assignable = requires(I i, bool b) { *i = b; };

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
			dyn64 b(3);
			REQUIRE_EQ(b.capacity_in_bits(), 3 * 64);
			for (size_t i = 0; i < b.capacity_in_bits(); ++i) {
				CHECK_FALSE(b.test(i));
			}
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
		using fixed64 = bitset<4, 4 * 64>; // extent stays in segments; 2nd param is now a bit count
		constexpr size_t fixed64_capacity_bits = 4 * 64;

		SUBCASE("initializer list must match extent exactly") {
			fixed64 b{1, 2, 3, 4};
			REQUIRE_EQ(b.size_in_bits(), 4 * 64);
			CHECK(b.test(0));   // segment 0, value 1
			CHECK(b.test(65));  // segment 1, offset 1, value 2
			CHECK(b.test(129)); // segment 2, offset 1, value 3
		}

		SUBCASE("mismatched initializer list size throws") {
			CHECK_THROWS_AS((fixed64{1, 2, 3}), std::length_error);
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
			dyn8 b{0x00};
			CHECK_NOTHROW((dyn8::bit_iterator{b, 7}));
			CHECK_THROWS_AS((dyn8::bit_iterator{b, 8}), std::out_of_range); // dyn8's segment width is 8 bits

			dyn8::bit_iterator it{b, 3};
			CHECK(it == b.begin() + 3);
		}

		SUBCASE("explicit iterator construction with an offset and segment validates both") {
			dyn8 b{0x00, 0x00};
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
			for (auto it = b.rbegin(); it != b.rend(); ++it, --global_ix) {
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
			for (auto it = b.rbegin(); it != b.rend(); ++it) {
				++visited;
			}
			CHECK_EQ(visited, b.capacity_in_bits());
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
			dyn8 b{0x00, 0x00, 0x00};
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
			dyn8 b{0x00, 0x00};
			auto const it = b.begin() + 2;

			CHECK((5 + it) == (it + 5));
			CHECK_EQ((5 + it).get(), (it + 5).get());
		}

		SUBCASE("free operator- : n - it == it - n (library-defined symmetric semantics)") {
			dyn8 b{0x00, 0x00};
			auto const it = b.begin() + 10;

			CHECK((3 - it) == (it - 3));
		}

		SUBCASE("default-constructed iterators are equality-comparable and independent of any bitset") {
			dyn8::bit_iterator a{};
			dyn8::bit_iterator b{};
			CHECK(a == b);

			dyn8 bs{0x00};
			auto valid = bs.begin();
			a = valid;
			CHECK(a == valid);
			CHECK(a != b);
		}

		SUBCASE("negative offsets: it += (-n) matches it -= n, it -= (-n) matches it += n") {
			dyn8 b{0x00, 0x00};
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
			dyn8 b{0x00, 0x00, 0x00};
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
			dyn8 b{0x00, 0x00, 0x00};
			auto const it = b.begin() + 5;

			auto const next5 = std::next(it, 5);
			CHECK_EQ(next5, b.begin() + 10);

			auto const prev3 = std::prev(it, 3);
			CHECK_EQ(prev3, b.begin() + 2);

			CHECK_EQ(std::distance(prev3, next5), 8);
			CHECK_EQ(std::distance(next5, prev3), -8);
		}

		SUBCASE("std::advance with a negative distance moves backwards correctly") {
			dyn8 b{0x00, 0x00, 0x00};
			auto it = b.begin() + 12;
			std::advance(it, -5);
			CHECK_EQ(it, b.begin() + 7);
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
	}

	TEST_CASE("fixed and bounded capacity - equality, bitwise ops and shifts") {
		using fixed64 = bitset<4, 4 * 64>; // extent stays in segments; 2nd param is now a bit count

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
	    dyn64 b_long(32); // parens - see the "size constructor zero-fills every segment" note above
	    MESSAGE("hex (default): ", std::format("{}", b_long));
	    MESSAGE("binary: ", std::format("{:b}", b_long));
	}

	TEST_CASE("formatter output content (hex and binary, big endian)") {
		// bit_iterator itself walks LEAST-significant bit first: offset 0 is bit 0 of a segment
		// (segment_set uses "1 << offset", so offset 0 is the LSB), and ++it moves toward higher
		// offsets, i.e. toward the MSB. Printing must NOT follow that raw traversal order - a
		// human reads/writes binary and hex MSB-first - so the formatter reverses direction for
		// display. This is the specific behavior under test here, primarily in binary mode, where
		// the formatter explicitly does `subrange(it, seg_end) | std::views::reverse` before
		// emitting characters (bitset.hpp's format()). Segments themselves are still emitted in
		// storage order (segment 0 first) - only the bit order *within* a segment is reversed.

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

		SUBCASE("binary mode reverses within each segment independently, segments stay in storage order") {
			dyn8 b{0x00, 0xFF};
			CHECK_EQ(std::format("{:b}", b), "[\n[00000000]\n[11111111]\n]\n");
		}

		SUBCASE("hex mode renders one segment per line, in storage order") {
			// hex mode reads the raw segment value via it.get() and lets std::format's own hex
			// notation render it - it does not iterate bit-by-bit, so there is no LSB/MSB
			// traversal to reverse here. The MSB-first digit order (0xa5, not 0x5a) simply comes
			// from std::format's normal hex formatting of an integer, not from bitset's own logic.
			dyn8 b{0x12, 0x34};
			CHECK_EQ(std::format("{}", b), "[\n[0x12]\n[0x34]\n]\n");
		}

		SUBCASE("hex mode drops a fully-zero segment's leading zero (no fixed-width zero padding)") {
			// documents the actual current width behavior rather than assuming it: the format
			// spec's width only reserves 2*sizeof(T) characters, which already covers "0x" plus
			// every significant digit for a non-zero byte, but is one digit short of the full
			// 2-digit zero-padded form for a segment that is exactly 0.
			dyn8 b{0x00, 0xFF};
			CHECK_EQ(std::format("{}", b), "[\n[0x0]\n[0xff]\n]\n");
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
		using fixed64 = bitset<4, 4 * 64>; // extent stays in segments; 2nd param is now a bit count
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
			using fixed_odd = bitset<2, 100>;

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
		SUBCASE("drops a trailing not-fully-set segment") {
			dyn8 b{0xFF, 0xFF, 0x0F};
			b.shrink_to_fit();
			REQUIRE_EQ(b.capacity_in_bits(), 2 * 8);
			for (size_t i = 0; i < b.capacity_in_bits(); ++i) {
				CHECK(b.test(i));
			}
		}

		SUBCASE("skips over fully-set trailing segments to find the first non-full one") {
			// scanning from the end: segments 3 and 2 are fully set (0xFF) and get skipped over;
			// segment 1 (0x0F) is the first not-fully-set segment found, so storage is truncated
			// to exclude it and everything after it, keeping only segment 0.
			dyn8 b{0xFF, 0x0F, 0xFF, 0xFF};
			b.shrink_to_fit();
			REQUIRE_EQ(b.capacity_in_bits(), 1 * 8);
			for (size_t i = 0; i < b.capacity_in_bits(); ++i) {
				CHECK(b.test(i));
			}
		}

		SUBCASE("no shrink happens when every segment is fully set") {
			dyn8 b{0xFF, 0xFF, 0xFF};
			b.shrink_to_fit();
			CHECK_EQ(b.capacity_in_bits(), 3 * 8);
			CHECK_EQ(b.count(), 24);
		}

		SUBCASE("bounded (max-extent) capacity uses resize instead of storage reconstruction") {
			bounded64 b{~0ull, 0x0Full};
			b.shrink_to_fit();
			REQUIRE_EQ(b.capacity_in_bits(), bounded64_segment_bits);
			CHECK_EQ(b.count(), 64);
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
			dyn64 b(2); // parens - see the "size constructor zero-fills every segment" note above
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
			using fixed_odd = bitset<4, 200>;
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

		SUBCASE("reset(ix) beyond current logical size grows logical size, bit stays low") {
			dyn8 b{};
			b.reset(10);
			CHECK_EQ(b.size_in_bits(), 11);
			CHECK_FALSE(b.test(10));
		}

		SUBCASE("flip(ix) beyond current logical size grows logical size and flips the implicit 0 to 1") {
			dyn8 b{};
			b.flip(10);
			CHECK_EQ(b.size_in_bits(), 11);
			CHECK(b.test(10));
		}

		SUBCASE("set_first_free() grows logical size by exactly 1 when every existing bit is full") {
			dyn8 b(1); // 1 segment pre-allocated, all zero, logical size 8
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
			dyn64 b(1); // 1 segment, logical size 64
			b.set_positions(std::array{5uz, 70uz, 3uz}); // 70 is neither first nor last in the list
			CHECK_EQ(b.size_in_bits(), 71);
			CHECK(b.test(5));
			CHECK(b.test(70));
			CHECK(b.test(3));
		}

		SUBCASE("reset_positions() also grows logical size for out-of-range positions") {
			dyn64 b(1);
			b.reset_positions(std::array{5uz, 70uz});
			CHECK_EQ(b.size_in_bits(), 71);
			CHECK_FALSE(b.test(70));
		}
	}

	TEST_CASE("a fully static bitset's logical size never changes") {
		using fixed64 = bitset<4, 4 * 64>;

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
		SUBCASE("dropping a trailing not-fully-set segment shrinks logical size to the new exact capacity") {
			dyn8 b{0xFF, 0x0F}; // segment 1 (0x0F) is not fully set
			b.shrink_to_fit();
			CHECK_EQ(b.size_in_bits(), 8);
			CHECK_EQ(b.capacity_in_bits(), 8);
		}

		SUBCASE("no shrink happens when every segment is fully set - logical size is unchanged") {
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
			dyn8 b(1); // 1 full segment, 8 logical bits
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

		dyn8 b(2);
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
			dyn64 b{1ull << 5, 0}; // 2 full segments (128 bits, aligned); bit 5 is the first 1
			CHECK_EQ(b.countr_zero(), 5);
		}

		SUBCASE("aligned: an entirely-zero bitset returns exactly the (segment-aligned) logical size") {
			dyn64 b{0, 0};
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
			using fixed_odd = bitset<2, 100>;
			fixed_odd b{0, 1ull << 26}; // offset 26 within the 36-bit leftover (global bit 90)
			REQUIRE_EQ(b.size_in_bits(), 100);
			CHECK_EQ(b.countr_zero(), seg64 + 26);
		}
	}

	TEST_CASE("countl_zero honors an exact (non-segment-aligned) logical size") {
		constexpr size_t seg64 = 64;

		SUBCASE("aligned: the set bit sits in the last segment (non-edge - resolved without any fallback)") {
			dyn64 b{0, 1ull << 58}; // 2 full segments (128 bits, aligned)
			CHECK_EQ(b.countl_zero(), 5); // 63 - 58
		}

		SUBCASE("aligned: an entirely-zero bitset returns exactly the (segment-aligned) logical size") {
			dyn64 b{0, 0};
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
			using fixed_odd = bitset<2, 100>;
			fixed_odd b{1ull << 10, 0}; // leftover (segment 1) all zero; segment 0 decides the answer
			REQUIRE_EQ(b.size_in_bits(), 100);
			CHECK_EQ(b.countl_zero(), 36 + (seg64 - 1 - 10)); // 36 leftover zeros + segment 0's leading zeros
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
			using fixed_odd = bitset<2, 100>;
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
			dyn64 b{0, 0};
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
			using fixed_odd = bitset<2, 100>;
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
}
