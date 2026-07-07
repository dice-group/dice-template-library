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
				REQUIRE(executed);
			}

			SUBCASE("fail") {
				try {
					DICE_DEFER { executed = true; };
					throw std::runtime_error{""};
				} catch (...) {
					// expecting excetpion
				}

				REQUIRE(executed);
			}
		}

		TEST_CASE("fail") {
			bool executed = false;

			SUBCASE("success") {
				{
					DICE_DEFER_TO_FAIL { executed = true; };
				}
				REQUIRE_FALSE(executed);
			}

			SUBCASE("fail") {
				try {
					DICE_DEFER_TO_FAIL { executed = true; };
					throw std::runtime_error{""};
				} catch (...) {
					// expecting excetpion
				}

				REQUIRE(executed);
			}
		}

		TEST_CASE("success") {
			bool executed = false;

			SUBCASE("success") {
				{
					DICE_DEFER_TO_SUCCESS { executed = true; };
				}
				REQUIRE(executed);
			}

			SUBCASE("fail") {
				try {
					DICE_DEFER_TO_SUCCESS { executed = true; };
					throw std::runtime_error{""};
				} catch (...) {
					// expecting excetpion
				}

				REQUIRE_FALSE(executed);
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

    TEST_CASE("move if value") {
	    int value = 0;
	    int &ref = value;
	    int const &cref = value;
	    int &&rref = std::move(value);
	    int const &&crref = std::move(value);

	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(value)), int &&>); // move
	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(ref)), int &>); // no move
	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(cref)), int const &>); // no move
	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(rref)), int &&>); // move
	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(crref)), int const &&>); // no move
	}
} // namespace dice::template_library
