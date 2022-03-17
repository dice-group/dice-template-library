#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <Dice/IntegralTemplatedTuple.hpp>
#include <doctest/doctest.h>

#include <algorithm>
#include <iostream>

namespace Dice::templateLibrary {

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
	auto get(std::tuple<Args...> &t) {
		return std::get<I>(t);
	}

	template<auto I, typename T>
	auto get(T &t) {
		return t.template get<I>();
	}

	template<auto Start, typename TupleLike, typename INTEGER, INTEGER... IDS>
	std::vector<int> toIntVector(TupleLike const &tup, std::integer_sequence<INTEGER, IDS...>) {
		return {get<Start + IDS>(tup)...};
	}

	template<auto Start, auto End, typename TupleLike>
	auto toIntVector(TupleLike const &tup) {
		static constexpr auto Count = End - Start + 1;
		return toIntVector<Start>(tup, std::make_integer_sequence<decltype(Start), Count>{});
	}

	template<auto N>
	struct Data {
		operator int() const { return static_cast<int>(N); }
	};

	TEST_SUITE("checking test utilities") {
		TEST_CASE("toVector test") {
			std::tuple<int, int, int> tup{2, 3, 4};
			auto vec = toIntVector<0, 2>(tup);
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
			auto vec = toIntVector<0, 2>(tup);
			REQUIRE(is_equal(vec, {0, 1, 2}));
		}
	}

	TEST_SUITE("testing of the integral templated tuple") {
		TEST_CASE_TEMPLATE_DEFINE("Is same for single value at 0", T, singleAtZero) {
			IntegralTemplatedTuple<Data, 0, 0> inTuple;
			auto vec = toIntVector<0, 0>(inTuple);
			REQUIRE(is_equal(vec, {0}));
		}

		TEST_CASE_TEMPLATE_DEFINE("Is same for single value at 0", T, singleAtFour) {
			IntegralTemplatedTuple<Data, 4, 4> inTuple;
			auto vec = toIntVector<4, 4>(inTuple);
			REQUIRE(is_equal(vec, {4}));
		}

		TEST_CASE_TEMPLATE_DEFINE("Is same for multiple values starting at 0", T, multipleAtZero) {
			IntegralTemplatedTuple<Data, 0, 4> inTuple;
			auto vec = toIntVector<0, 4>(inTuple);
			REQUIRE(is_equal(vec, {0, 1, 2, 3, 4}));
		}

		TEST_CASE_TEMPLATE_DEFINE("Is same for multiple values starting at 4", T, multipleAtFour) {
			IntegralTemplatedTuple<Data, 4, 8> inTuple;
			auto vec = toIntVector<4, 8>(inTuple);
			REQUIRE(is_equal(vec, {4, 5, 6, 7, 8}));
		}

		TEST_CASE_TEMPLATE_DEFINE("Is same for multiple values counting down", T, multipleDown) {
			IntegralTemplatedTuple<Data, 8, 4> inTuple;
			auto vec = toIntVector<4, 8>(inTuple);
			REQUIRE(is_equal(vec, {4, 5, 6, 7, 8}));
		}

		TEST_CASE_TEMPLATE_DEFINE("Negative values counting up", T, negCountingUp) {
			IntegralTemplatedTuple<Data, -2, 2> inTuple;
			auto vec = toIntVector<-2, 2>(inTuple);
			REQUIRE(is_equal(vec, {-2, -1, 0, 1, 2}));
		}

		TEST_CASE_TEMPLATE_DEFINE("Negative values counting down", T, negCountingDown) {
			IntegralTemplatedTuple<Data, 2, -2> inTuple;
			auto vec = toIntVector<-2, 2>(inTuple);
			REQUIRE(is_equal(vec, {-2, -1, 0, 1, 2}));
		}

		TEST_CASE_TEMPLATE_DEFINE("reinterpret_cast counting up", T, recastUp) {
			IntegralTemplatedTuple<Data, 0, 3> inTuple;
			// NOLINTNEXTLINE
			auto *casted = reinterpret_cast<IntegralTemplatedTuple<Data, 0, 2> *>(&inTuple);
			auto vec = toIntVector<0, 2>(*casted);
			REQUIRE(is_equal(vec, {0, 1, 2}));
		}

		TEST_CASE_TEMPLATE_DEFINE("reinterpret_cast counting down", T, recastDown) {
			IntegralTemplatedTuple<Data, 3, 0> inTuple;
			// NOLINTNEXTLINE
			auto *casted = reinterpret_cast<IntegralTemplatedTuple<Data, 3, 1> *>(&inTuple);
			auto vec = toIntVector<1, 3>(*casted);
			REQUIRE(is_equal(vec, {1, 2, 3}));
		}

		TEST_CASE_TEMPLATE_DEFINE("reinterpret_cast counting up from negative", T,
								  recastUpFromNeg) {
			IntegralTemplatedTuple<Data, -1, 3> inTuple;
			// NOLINTNEXTLINE
			auto *casted = reinterpret_cast<IntegralTemplatedTuple<Data, -1, 2> *>(&inTuple);
			auto vec = toIntVector<-1, 2>(*casted);
			REQUIRE(is_equal(vec, {-1, 0, 1, 2}));
		}

		TEST_CASE_TEMPLATE_DEFINE("reinterpret_cast counting down into negative", T,
								  recastDownToNeg) {
			IntegralTemplatedTuple<Data, 3, -2> inTuple;
			// NOLINTNEXTLINE
			auto *casted = reinterpret_cast<IntegralTemplatedTuple<Data, 3, -1> *>(&inTuple);
			auto vec = toIntVector<-1, 3>(*casted);
			REQUIRE(is_equal(vec, {-1, 0, 1, 2, 3}));
		}

		TEST_CASE_TEMPLATE_INVOKE(singleAtZero, unsigned, short);
		TEST_CASE_TEMPLATE_INVOKE(singleAtFour, unsigned, short);
		TEST_CASE_TEMPLATE_INVOKE(multipleAtZero, unsigned, short);
		TEST_CASE_TEMPLATE_INVOKE(multipleAtFour, unsigned, short);
		TEST_CASE_TEMPLATE_INVOKE(multipleDown, unsigned, short);
		TEST_CASE_TEMPLATE_INVOKE(negCountingUp, short);
		TEST_CASE_TEMPLATE_INVOKE(negCountingDown, short);

		TEST_CASE_TEMPLATE_INVOKE(recastUp, unsigned, short);
		TEST_CASE_TEMPLATE_INVOKE(recastDown, unsigned, short);
		TEST_CASE_TEMPLATE_INVOKE(recastUpFromNeg, short);
		TEST_CASE_TEMPLATE_INVOKE(recastDownToNeg, short);
	}

}// namespace Dice::templateLibrary
