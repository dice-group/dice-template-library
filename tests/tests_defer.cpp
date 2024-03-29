#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/defer.hpp>

#include <doctest/doctest.h>

namespace dice::template_library {
	TEST_SUITE("defer") {
		TEST_CASE("always") {
			bool executed = false;

			SUBCASE("success") {
				{
					DICE_DEFER { executed = true; };
				}
				CHECK(executed);
			}

			SUBCASE("fail") {
				try {
					DICE_DEFER { executed = true; };
					throw std::runtime_error{""};
				} catch (...) {
					// expecting excetpion
				}

				CHECK(executed);
			}
		}

		TEST_CASE("fail") {
			bool executed = false;

			SUBCASE("success") {
				{
					DICE_DEFER_TO_FAIL { executed = true; };
				}
				CHECK_FALSE(executed);
			}

			SUBCASE("fail") {
				try {
					DICE_DEFER_TO_FAIL { executed = true; };
					throw std::runtime_error{""};
				} catch (...) {
					// expecting excetpion
				}

				CHECK(executed);
			}
		}

		TEST_CASE("success") {
			bool executed = false;

			SUBCASE("success") {
				{
					DICE_DEFER_TO_SUCCESS { executed = true; };
				}
				CHECK(executed);
			}

			SUBCASE("fail") {
				try {
					DICE_DEFER_TO_SUCCESS { executed = true; };
					throw std::runtime_error{""};
				} catch (...) {
					// expecting excetpion
				}

				CHECK_FALSE(executed);
			}
		}

		TEST_CASE("multiple in same scope") {
			// only checking if it compiles
			DICE_DEFER {};
			DICE_DEFER {};
			DICE_DEFER_TO_FAIL {};
			DICE_DEFER_TO_FAIL {};
			DICE_DEFER_TO_SUCCESS {};
			DICE_DEFER_TO_SUCCESS {};
		}
	}
} // namespace dice::template_library
