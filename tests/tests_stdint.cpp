#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/stdint.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

TEST_SUITE("stdint") {
	using namespace dice::template_library::literals;

	TEST_CASE("unsigned") {
		static_assert(std::is_same_v<decltype(123_u8), uint8_t>);
		static_assert(std::is_same_v<decltype(123_u16), uint16_t>);
		static_assert(std::is_same_v<decltype(123_u32), uint32_t>);
		static_assert(std::is_same_v<decltype(123_u64), uint64_t>);
		static_assert(std::is_same_v<decltype(123_usize), size_t>);
	}

	TEST_CASE("signed positive") {
		static_assert(std::is_same_v<decltype(123_i8), int8_t>);
		static_assert(std::is_same_v<decltype(123_i16), int16_t>);
		static_assert(std::is_same_v<decltype(123_i32), int32_t>);
		static_assert(std::is_same_v<decltype(123_i64), int64_t>);
		static_assert(std::is_same_v<decltype(123_isize), ptrdiff_t>);
	}

	TEST_CASE("signed negative") {
		// Note: due to integer promotion this does not work because -123_i{8,16} get promoted to int.
		// There is nothing we can do about it because user defined literals do not accept signed integers.
		//
		//static_assert(std::is_same_v<decltype(-123_i8), int8_t>);
		//static_assert(std::is_same_v<decltype(-123_i16), int16_t>);

		static_assert(std::is_same_v<decltype(-123_i32), int32_t>);
		static_assert(std::is_same_v<decltype(-123_i64), int64_t>);
		static_assert(std::is_same_v<decltype(-123_isize), ptrdiff_t>);
	}
}
