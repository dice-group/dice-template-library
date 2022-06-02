#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <Dice/template-library/switch_template_functions.hpp>

#include <doctest/doctest.h>

namespace Dice::template_library {
	TEST_SUITE("testing of the compiled switch") {
		const int value = 1;
		template<int i>
		static constexpr int compiled_identity = i;

		TEST_CASE("no return") {
			int res = value + 1;
			switch_cases<-5, 5>(value, [&res](auto i) { res = compiled_identity<i>; });
			REQUIRE_EQ(value, res);
		}

		TEST_CASE("no return, negative value") {
			int res = value + 1;
			switch_cases<-5, 5>(-value, [&res](auto i) { res = compiled_identity<i>; });
			REQUIRE_EQ(-value, res);
		}

		TEST_CASE("no return, implicitly starting at 0") {
			int res = value + 1;
			switch_cases<5>(value, [&res](auto i) { res = compiled_identity<i>; });
			REQUIRE_EQ(value, res);
		}

		TEST_CASE("no return, negative value, implicitly starting at 0") {
			int res = value + 1;
			switch_cases<-5>(-value, [&res](auto i) { res = compiled_identity<i>; });
			REQUIRE_EQ(-value, res);
		}

		TEST_CASE("return") {
			auto res = switch_cases<-5, 5>(
					value, [](auto i) { return compiled_identity<i>; }, []() { return value + 1; });
			REQUIRE_EQ(value, res);
		}

		TEST_CASE("return, no explicit default") {
			auto res = switch_cases<-5, 5>(value, [](auto i) { return compiled_identity<i>; });
			REQUIRE_EQ(value, res);
		}

		TEST_CASE("return, no explicit default, implicitly starting at 0") {
			auto res = switch_cases<5>(value, [](auto i) { return compiled_identity<i>; });
			REQUIRE_EQ(value, res);
		}

		TEST_CASE("return, negative value") {
			auto res = switch_cases<-5, 5>(
					-value, [](auto i) { return compiled_identity<i>; }, []() { return value + 1; });
			REQUIRE_EQ(-value, res);
		}

		TEST_CASE("return, implicitly starting at 0") {
			auto res = switch_cases<5>(
					value, [](auto i) { return compiled_identity<i>; }, []() { return value + 1; });
			REQUIRE_EQ(value, res);
		}

		TEST_CASE("return, negative value, implicitly starting at 0") {
			auto res = switch_cases<-5>(
					-value, [](auto i) { return compiled_identity<i>; }, []() { return value + 1; });
			REQUIRE_EQ(-value, res);
		}

		TEST_CASE("default is run when the parameter is not in range") {
			auto res = switch_cases<-5>(
					value, [](auto i) { return compiled_identity<i>; }, []() { return value + 1; });
			REQUIRE_EQ(value + 1, res);
		}
	}

	TEST_SUITE("references") {
		template<int N>
		class TestType {
			// NOLINTNEXTLINE
			static inline int data = N;
			static void reset() noexcept {
				data = N;
			}

		public:
			static int &get() noexcept {
				return data;
			}
			static void resetAll() noexcept {
				if constexpr (N > 0) {
					TestType<N - 1>::resetAll();
				}
				reset();
			}
		};

		TEST_CASE("Test type works") {
			TestType<5>::resetAll();
			REQUIRE_EQ(5, TestType<5>::get());
		}

		TEST_CASE("simple return") {
			TestType<5>::resetAll();
			static constexpr int value = 1;
			int &res_ref = switch_cases<0, 5>(
					value, [](auto i) -> auto & { return TestType<i>::get(); });
			res_ref += 1;
			REQUIRE_EQ(res_ref, TestType<value>::get());
		}
	}
}// namespace Dice::template_library
