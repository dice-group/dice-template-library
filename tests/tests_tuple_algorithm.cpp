#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/tuple_algorithm.hpp>

namespace dice::template_library {

	TEST_SUITE("Tuple algorithm") {
		TEST_CASE("tuple_fold") {
			auto res = tuple_fold(std::make_tuple(1, 1.2, 2ull), 0.0, [](auto acc, auto x) {
				return acc + static_cast<double>(x);
			});

			CHECK_EQ(res, 4.2);
		}

		TEST_CASE("tuple_type_fold") {
			auto res = tuple_type_fold<std::tuple<int, double, long>>(std::string{""}, []<typename T>(auto acc) {
				return acc + typeid(T).name();
			});

			CHECK_EQ(res, "idl");
		}

		TEST_CASE("tuple_for_each") {
			double res = 0.0;

			tuple_for_each(std::make_tuple(1, 1.2, 2ull), [&](auto x) {
				res += static_cast<double>(x);
			});

			CHECK_EQ(res, 4.2);
		}

		TEST_CASE("tuple_type_for_each") {
			std::string res;

			tuple_type_for_each<std::tuple<int, double, long>>([&]<typename T>() {
				res += typeid(T).name();
			});

			CHECK_EQ(res, "idl");
		}
	}

} // namespace dice::template_library
