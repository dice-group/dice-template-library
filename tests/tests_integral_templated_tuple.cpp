#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/integral_template_tuple.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <iostream>

namespace dice::template_library {

	template<typename T>
	std::ostream &operator<<(std::ostream &os, std::vector<T> const &v) {
		if (v.empty()) { return os << "[]"; }
		os << '[';
		std::for_each(v.begin(), v.end() - 1, [&os](T const &data) { os << data << ", "; });
		return os << v[v.size() - 1] << ']';
	}

	template<typename T>
	bool is_equal_silent(std::vector<T> const &lhs, std::vector<T> const &rhs) noexcept {
		if (lhs.size() != rhs.size()) { return false; }
		for (std::size_t i = 0; i < lhs.size(); ++i) {
			if (lhs[i] != rhs[i]) { return false; }
		}
		return true;
	}

	template<typename T>
	bool is_equal(std::vector<T> const &lhs, std::vector<T> const &rhs) noexcept {
		if (!is_equal_silent(lhs, rhs)) {
			std::cout << lhs << " != " << rhs << '\n';
			return false;
		}
		return true;
	}

	//Needed to unify the access to a std::tuple and our integral version
	template<auto I, typename... Args>
	decltype(auto) get(std::tuple<Args...> &t) {
		return std::get<I>(t);
	}

	template<auto I, typename T>
	decltype(auto) get(T &t) {
		return t.template get<I>();
	}

	template<auto Start, typename TupleLike, typename INTEGER, INTEGER... IDS>
	std::vector<int> to_int_vector(TupleLike const &tup, std::integer_sequence<INTEGER, IDS...>) {
		return {get<Start + IDS>(tup)...};
	}

	template<auto Start, auto End, typename TupleLike>
	auto to_int_vector(TupleLike const &tup) {
		static constexpr auto Count = End - Start + 1;
		return to_int_vector<Start>(tup, std::make_integer_sequence<decltype(Start), Count>{});
	}

	template<auto N>
	struct Data {
		operator int() const { return static_cast<int>(N); }
	};

	TEST_SUITE("checking test utilities") {
		TEST_CASE("toVector test") {
			std::tuple<int, int, int> tup{2, 3, 4};
			auto vec = to_int_vector<0, 2>(tup);
			REQUIRE(vec[0] == 2);
			REQUIRE(vec[1] == 3);
			REQUIRE(vec[2] == 4);
		}

		TEST_CASE("vector is_equal test") {
			std::vector<int> vec{1, 2, 3};
			REQUIRE(is_equal_silent(vec, {1, 2, 3}));
			REQUIRE_FALSE(is_equal_silent(vec, {}));
			REQUIRE_FALSE(is_equal_silent(vec, {2, 3}));
			REQUIRE_FALSE(is_equal_silent(vec, {1, 2, 3, 4}));
		}

		TEST_CASE("Data tuple an be converted to int") {
			std::tuple<Data<0>, Data<1>, Data<2>> tup;
			auto vec = to_int_vector<0, 2>(tup);
			REQUIRE(is_equal(vec, {0, 1, 2}));
		}
	}

	TEST_SUITE("testing of the integral templated tuple") {
		TEST_CASE("Is same for single value at 0") {
			integral_template_tuple<Data, 0, 0> inTuple;
			auto vec = to_int_vector<0, 0>(inTuple);
			REQUIRE(is_equal(vec, {0}));
		}

		TEST_CASE("Is same for single value at 0") {
			integral_template_tuple<Data, 4, 4> inTuple;
			auto vec = to_int_vector<4, 4>(inTuple);
			REQUIRE(is_equal(vec, {4}));
		}

		TEST_CASE("Is same for multiple values starting at 0") {
			integral_template_tuple<Data, 0, 4> inTuple;
			auto vec = to_int_vector<0, 4>(inTuple);
			REQUIRE(is_equal(vec, {0, 1, 2, 3, 4}));
		}

		TEST_CASE("Is same for multiple values starting at 4") {
			integral_template_tuple<Data, 4, 8> inTuple;
			auto vec = to_int_vector<4, 8>(inTuple);
			REQUIRE(is_equal(vec, {4, 5, 6, 7, 8}));
		}

		TEST_CASE("Is same for multiple values counting down") {
			integral_template_tuple<Data, 8, 4> inTuple;
			auto vec = to_int_vector<4, 8>(inTuple);
			REQUIRE(is_equal(vec, {4, 5, 6, 7, 8}));
		}

		TEST_CASE("Negative values counting up") {
			integral_template_tuple<Data, -2, 2> inTuple;
			auto vec = to_int_vector<-2, 2>(inTuple);
			REQUIRE(is_equal(vec, {-2, -1, 0, 1, 2}));
		}

		TEST_CASE("Negative values counting down") {
			integral_template_tuple<Data, 2, -2> inTuple;
			auto vec = to_int_vector<-2, 2>(inTuple);
			REQUIRE(is_equal(vec, {-2, -1, 0, 1, 2}));
		}

		TEST_CASE("reinterpret_cast counting up") {
			integral_template_tuple<Data, 0, 3> inTuple;
			// NOLINTNEXTLINE
			auto *casted = reinterpret_cast<integral_template_tuple<Data, 0, 2> *>(&inTuple);
			auto vec = to_int_vector<0, 2>(*casted);
			REQUIRE(is_equal(vec, {0, 1, 2}));
		}

		TEST_CASE("reinterpret_cast counting down") {
			integral_template_tuple<Data, 3, 0> inTuple;
			// NOLINTNEXTLINE
			auto *casted = reinterpret_cast<integral_template_tuple<Data, 3, 1> *>(&inTuple);
			auto vec = to_int_vector<1, 3>(*casted);
			REQUIRE(is_equal(vec, {1, 2, 3}));
		}

		TEST_CASE("reinterpret_cast counting up from negative") {
			integral_template_tuple<Data, -1, 3> inTuple;
			// NOLINTNEXTLINE
			auto *casted = reinterpret_cast<integral_template_tuple<Data, -1, 2> *>(&inTuple);
			auto vec = to_int_vector<-1, 2>(*casted);
			REQUIRE(is_equal(vec, {-1, 0, 1, 2}));
		}

		TEST_CASE("reinterpret_cast counting down into negative") {
			integral_template_tuple<Data, 3, -2> inTuple;
			// NOLINTNEXTLINE
			auto *casted = reinterpret_cast<integral_template_tuple<Data, 3, -1> *>(&inTuple);
			auto vec = to_int_vector<-1, 3>(*casted);
			REQUIRE(is_equal(vec, {-1, 0, 1, 2, 3}));
		}
	}

}// namespace dice::template_library
