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

	TEST_SUITE("lazy_switch_default") {
		template<typename T>
		struct int_result {
			using type = int;
		};

		template<typename T>
		struct double_result {
			using type = double;
		};

		template<typename T>
		struct void_result {
			using type = void;
		};

		template<typename T>
		struct error_result {
			static_assert(sizeof(T) == 0, "Should never be instantiated");
			using type = void;
		};

		TEST_CASE("first case matches") {
			using result = lazy_switch_default_t<
					error_result<int>,
					case_<true, int_result<int>>,
					case_<true, double_result<int>>>;
			REQUIRE(std::is_same_v<result, int>);
		}

		TEST_CASE("second case matches") {
			using result = lazy_switch_default_t<
					error_result<int>,
					case_<false, int_result<int>>,
					case_<true, double_result<int>>>;
			REQUIRE(std::is_same_v<result, double>);
		}

		TEST_CASE("default case when no match") {
			using result = lazy_switch_default_t<
					void_result<int>,
					case_<false, int_result<int>>,
					case_<false, double_result<int>>>;
			REQUIRE(std::is_same_v<result, void>);
		}

		template<typename T>
		using select = lazy_switch_default_t<
				error_result<T>,
				case_<std::is_integral_v<T>, int_result<T>>,
				case_<std::is_floating_point_v<T>, double_result<T>>,
				case_<std::is_void_v<T>, void_result<T>>>;

		TEST_CASE("type trait based selection") {
			REQUIRE(std::is_same_v<select<int>, int>);
			REQUIRE(std::is_same_v<select<float>, double>);
			REQUIRE(std::is_same_v<select<void>, void>);
		}
	}

	TEST_SUITE("lazy_switch") {
		template<typename T>
		struct int_result {
			using type = int;
		};

		template<typename T>
		struct double_result {
			using type = double;
		};

		TEST_CASE("first case matches") {
			using result = lazy_switch_t<
					case_<true, int_result<int>>,
					case_<false, double_result<int>>>;
			REQUIRE(std::is_same_v<result, int>);
		}

		TEST_CASE("second case matches") {
			using result = lazy_switch_t<
					case_<false, int_result<int>>,
					case_<true, double_result<int>>>;
			REQUIRE(std::is_same_v<result, double>);
		}

		template<typename T>
		using select_no_default = lazy_switch_t<
				case_<std::is_integral_v<T>, int_result<T>>,
				case_<std::is_floating_point_v<T>, double_result<T>>>;

		TEST_CASE("type trait based selection without default") {
			REQUIRE(std::is_same_v<select_no_default<int>, int>);
			REQUIRE(std::is_same_v<select_no_default<float>, double>);
		}
	}
}// namespace dice::template_library