#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/lazy_conditional.hpp>

#include <doctest/doctest.h>

#include <type_traits>

namespace dice::template_library {
	TEST_SUITE("lazy_conditional") {
		template<typename T>
				struct valid {
			using type = int;
		};

		template<typename T>
		struct invalid {
			static_assert(sizeof(T) == 0, "Should never be instantiated");
			using type = void;
		};

		TEST_CASE("select true branch - avoid static_assert in false branch") {
			// Would fail with std::conditional because it instantiates both branches
			using result = lazy_conditional_t<true, valid<void>, invalid<void>>;
			REQUIRE(std::is_same_v<result, int>);
		}

		TEST_CASE("select false branch - avoid static_assert in true branch") {


			// Would fail with std::conditional because it instantiates both branches
			using result = lazy_conditional_t<false, invalid<void>, valid<void>>;
			REQUIRE(std::is_same_v<result, int>);
		}

		template<typename T>
			struct get_value_type {
			using type = typename T::value_type;
		};

		template<typename T>
		struct identity {
			using type = T;
		};

		TEST_CASE("avoid accessing invalid type member") {
			// Would fail with std::conditional when T=int because int::value_type is invalid
			using result = lazy_conditional_t<std::is_class_v<int>, get_value_type<int>, identity<int>>;
			REQUIRE(std::is_same_v<result, int>);
		}
	}
} // namespace dice::template_library