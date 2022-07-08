#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/for.hpp>

#include <doctest/doctest.h>

namespace dice::template_library {
	TEST_SUITE("testing of compile time for_types") {
		TEST_CASE("sizeof fixed-sized types") {
			std::vector<size_t> sizes;

			for_types<uint8_t, uint16_t, uint32_t, uint64_t>([&sizes]<typename T>() {
				sizes.push_back(sizeof(T));
			});
			REQUIRE(sizes == std::vector<size_t>{1, 2, 4, 8});
		}
	}

	TEST_SUITE("testing of compile time for_values") {
		TEST_CASE("sum up some numbers") {
			ssize_t result = 0;
			for_values<-5, 2, 3, 4, 5, 6>([&result](auto x) {
				result += x;
			});
			REQUIRE(result == (-5 + 2 + 3 + 4 + 5 + 6));
		}
	}

	TEST_SUITE("testing of compile time for_range") {
		TEST_CASE("sum up some numbers (incr.)") {
			ssize_t result = 0;
			for_range<-5, 6>([&result](auto x) {
				result += x;
			});
			REQUIRE(result == 0);
		}

		TEST_CASE("sum up some numbers (decr.)") {
			ssize_t result = 0;
			for_range<5, -6>([&result](auto x) {
				result += x;
			});
			REQUIRE(result == 0);
		}
	}
}// namespace dice::template_library
