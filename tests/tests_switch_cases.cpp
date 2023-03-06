#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/switch_cases.hpp>

#include <doctest/doctest.h>

namespace dice::template_library {
	TEST_SUITE("testing of the compiled switch") {
		TEST_CASE("no return") {
			for (int value = -5; value < 5; ++value) {
				int res = -99;
				switch_cases<-5, 5>(value, [&res]<auto i>() { res = i; });
				REQUIRE_EQ(value, res);
			}
		}

		TEST_CASE("no return, implicitly starting at 0") {
			for (int value = 0; value < 5; ++value) {
				int res = -99;
				switch_cases<5>(value, [&res]<auto i>() { res = i; });
				REQUIRE_EQ(value, res);
			}
		}

		TEST_CASE("no return, negative value, implicitly starting at 0") {
			for (int value = -4; value >= 0; ++value) {
				int res = -99;
				switch_cases<-5>(value, [&res]<auto i>() { res = i; });
				REQUIRE_EQ(value, res);
			}
		}

		TEST_CASE("return") {
			for (int value = -5; value < 5; ++value) {
				auto res = switch_cases<-5, 5>(value,
						[]<auto i>() { return i; },
						[]() { return -99; });
				REQUIRE_EQ(value, res);
			}
		}

		TEST_CASE("return, no explicit default") {
			for (int value = -5; value < 5; ++value) {
				auto res = switch_cases<-5, 5>(value, []<auto i>() { return i; });
				REQUIRE_EQ(value, res);
			}
		}

		TEST_CASE("return, no explicit default, implicitly starting at 0") {
			for (int value = 0; value < 5; ++value) {
				auto res = switch_cases<5>(value, []<auto i>() { return i; });
				REQUIRE_EQ(value, res);
			}
		}

		TEST_CASE("return, implicitly starting at 0") {
			for (int value = 0; value < 5; ++value) {
				auto res = switch_cases<5>(value,
						[]<auto i>() { return i; },
						[]() { return -99; });
				REQUIRE_EQ(value, res);
			}
		}

		TEST_CASE("return, negative value, implicitly starting at 0") {
			for (int value = -5; value < 0; ++value) {
				auto res = switch_cases<-5>(value,
						[]<auto i>() { return i; },
						[]() { return -99; });
				REQUIRE_EQ(value, res);
			}
		}

		TEST_CASE("default is run when the parameter is not in range") {
			SUBCASE("positive range, too small") {
				auto res = switch_cases<1, 5>(0,
						[]<auto i>() { return i; },
						[]() { return -99; });
				REQUIRE_EQ(-99, res);
			}

			SUBCASE("positive range, too large") {
				auto res = switch_cases<1, 5>(5,
						[]<auto i>() { return i; },
						[]() { return -99; });
				REQUIRE_EQ(-99, res);
			}

			SUBCASE("negative range, too small") {
				auto res = switch_cases<-5>(-6,
						[]<auto i>() { return i; },
						[]() { return -99; });
				REQUIRE_EQ(-99, res);
			}

			SUBCASE("negative range, too large") {
				auto res = switch_cases<-5>(10,
						[]<auto i>() { return i; },
						[]() { return -99; });
				REQUIRE_EQ(-99, res);
			}
		}
	}

	TEST_SUITE("references") {
		template<int N>
		class test_type {
			// NOLINTNEXTLINE
			static inline int data = N;
			static void reset() noexcept {
				data = N;
			}

		public:
			static int &get() noexcept {
				return data;
			}
			static void reset_all() noexcept {
				if constexpr (N > 0) {
					test_type<N - 1>::reset_all();
				}
				reset();
			}
		};

		TEST_CASE("Test type works") {
			test_type<5>::reset_all();
			REQUIRE_EQ(5, test_type<5>::get());
		}

		TEST_CASE("simple return") {
			test_type<5>::reset_all();
			static constexpr int value = 1;
			int &res_ref = switch_cases<0, 5>(
					value, []<auto i>() -> auto & { return test_type<i>::get(); });
			res_ref += 1;
			REQUIRE_EQ(res_ref, test_type<value>::get());
		}
	}
}// namespace dice::template_library
