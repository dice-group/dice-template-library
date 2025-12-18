#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/integral_template_tuple_v2.hpp>

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
		static constexpr auto Count = End - Start;
		return to_int_vector<Start>(tup, std::make_integer_sequence<decltype(Start), Count>{});
	}

	template<auto N>
	struct Data {
		int value_;

		Data() = default;
		explicit Data(int value) : value_{value} {}

		operator int() const { return static_cast<int>(N); }
		auto operator<=>(Data const &other) const noexcept {
			return value_ <=> other.value_;
		}
	};

	TEST_SUITE("checking test utilities") {
		TEST_CASE("toVector test") {
			std::tuple<int, int, int> tup{2, 3, 4};
			auto vec = to_int_vector<0, 3>(tup);
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
			auto vec = to_int_vector<0, 3>(tup);
			REQUIRE(is_equal(vec, {0, 1, 2}));
		}
	}

	TEST_SUITE("testing of the integral templated tuple") {
		TEST_CASE("ctor") {
			integral_template_tuple_v2<0, 4, Data> tuple{individual_construct, 0, 1, 2, 3};
			REQUIRE(tuple.get<0>().value_ == 0);
			REQUIRE(tuple.get<1>().value_ == 1);
			REQUIRE(tuple.get<2>().value_ == 2);
			REQUIRE(tuple.get<3>().value_ == 3);

			integral_template_tuple_v2<0, 4, Data> tuple2{uniform_construct, 0};
			REQUIRE(tuple2.get<0>().value_ == 0);
			REQUIRE(tuple2.get<1>().value_ == 0);
			REQUIRE(tuple2.get<2>().value_ == 0);
			REQUIRE(tuple2.get<3>().value_ == 0);

			integral_template_tuple_v2<0, 4, Data> copy{tuple};
			tuple = copy;
			tuple = std::move(copy);

			integral_template_tuple_v2<0, 4, Data> moved{std::move(tuple2)};
		}

		TEST_CASE("comparision") {
			integral_template_tuple_v2<0, 4, Data> tuple{individual_construct, 0, 1, 2, 3};
			integral_template_tuple_v2<0, 4, Data> tuple2{individual_construct, 1, 2, 3, 4};

			REQUIRE(tuple <=> tuple2 == std::strong_ordering::less);
		}

		TEST_CASE("Is same for single value at 0") {
			integral_template_tuple_v2<0, 1, Data> inTuple;
			auto vec = to_int_vector<0, 1>(inTuple);
			REQUIRE(is_equal(vec, {0}));
		}

		TEST_CASE("Is same for single value at 4") {
			integral_template_tuple_v2<4, 5, Data> inTuple;
			auto vec = to_int_vector<4, 5>(inTuple);
			REQUIRE(is_equal(vec, {4}));
		}

		TEST_CASE("Is same for multiple values starting at 0") {
			integral_template_tuple_v2<0, 5, Data> inTuple;
			auto vec = to_int_vector<0, 5>(inTuple);
			REQUIRE(is_equal(vec, {0, 1, 2, 3, 4}));
		}

		TEST_CASE("Is same for multiple values starting at 4") {
			integral_template_tuple_v2<4, 9, Data> inTuple;
			auto vec = to_int_vector<4, 9>(inTuple);
			REQUIRE(is_equal(vec, {4, 5, 6, 7, 8}));
		}

		TEST_CASE("Negative values counting up") {
			integral_template_tuple_v2<-2, 3, Data> inTuple;
			auto vec = to_int_vector<-2, 3>(inTuple);
			REQUIRE(is_equal(vec, {-2, -1, 0, 1, 2}));
		}

		TEST_CASE("reinterpret_cast counting up") {
			integral_template_tuple_v2<0, 4, Data> inTuple;
			// NOLINTNEXTLINE
			integral_template_tuple_v2<1, 3, Data> &casted = inTuple.template subtuple<1, 3>();
			auto vec = to_int_vector<1, 3>(casted);
			REQUIRE(is_equal(vec, {1, 2}));
		}

		TEST_CASE("reinterpret_cast counting up from negative") {
			integral_template_tuple_v2<-1, 4, Data> inTuple;
			// NOLINTNEXTLINE
			integral_template_tuple_v2<0, 3, Data> &casted = inTuple.template subtuple<0, 3>();
			auto vec = to_int_vector<0, 3>(casted);
			REQUIRE(is_equal(vec, {0, 1, 2}));
		}

		TEST_CASE("size") {
			REQUIRE(integral_template_tuple_v2<-1, 2, Data>::size() == 3);
			REQUIRE(integral_template_tuple_v2<1, 2, Data>::size() == 1);
			REQUIRE(integral_template_tuple_v2<1, 3, Data>::size() == 2);
		}

		TEST_CASE("visit") {
			integral_template_tuple_v2<1, 3, Data> const tuple{};

			SUBCASE("non-void return") {
				auto const res = tuple.visit([acc = 0]<auto I>(Data<I> const &data) mutable {
					acc += static_cast<int>(data);
					return acc;
				});

				REQUIRE(res == 1 + 2);
			}

			SUBCASE("void return") {
				int acc = 0;
				tuple.visit([&acc]<auto I>(Data<I> const &data) {
					acc += static_cast<int>(data);
				});

				REQUIRE(acc == 1 + 2);
			}
		}
	}

	TEST_SUITE("testing of the integral templated tuple reverse") {
		TEST_CASE("ctor") {
			integral_template_tuple_rev_v2<3, -1, Data> tuple{individual_construct, 3, 2, 1, 0};
			REQUIRE(tuple.get<3>().value_ == 3);
			REQUIRE(tuple.get<2>().value_ == 2);
			REQUIRE(tuple.get<1>().value_ == 1);
			REQUIRE(tuple.get<0>().value_ == 0);

			integral_template_tuple_rev_v2<3, -1, Data> tuple2{uniform_construct, 0};
			REQUIRE(tuple2.get<3>().value_ == 0);
			REQUIRE(tuple2.get<2>().value_ == 0);
			REQUIRE(tuple2.get<1>().value_ == 0);
			REQUIRE(tuple2.get<0>().value_ == 0);

			integral_template_tuple_rev_v2<3, -1, Data> copy{tuple};
			tuple = copy;
			tuple = std::move(copy);

			integral_template_tuple_rev_v2<3, -1, Data> moved{std::move(tuple2)};
		}

		TEST_CASE("comparision") {
			integral_template_tuple_rev_v2<3, -1, Data> tuple{individual_construct, 3, 2, 1, 0};
			integral_template_tuple_rev_v2<3, -1, Data> tuple2{individual_construct, 4, 3, 2, 1};

			REQUIRE(tuple <=> tuple2 == std::strong_ordering::less);
		}

		TEST_CASE("Is same for single value at 4") {
			integral_template_tuple_rev_v2<4, 3, Data> inTuple;
			auto vec = to_int_vector<4, 5>(inTuple);
			REQUIRE(is_equal(vec, {4}));
		}

		TEST_CASE("Is same for multiple values counting down") {
			integral_template_tuple_rev_v2<8, 3, Data> inTuple;
			auto vec = to_int_vector<4, 9>(inTuple);
			REQUIRE(is_equal(vec, {4, 5, 6, 7, 8}));
		}

		TEST_CASE("Negative values counting down") {
			integral_template_tuple_rev_v2<2, -3, Data> inTuple;
			auto vec = to_int_vector<-2, 3>(inTuple);
			REQUIRE(is_equal(vec, {-2, -1, 0, 1, 2}));
		}

		TEST_CASE("reinterpret_cast counting down") {
			integral_template_tuple_rev_v2<3, -1, Data> inTuple;
			// NOLINTNEXTLINE
			integral_template_tuple_rev_v2<2, 0, Data> &casted = inTuple.template subtuple<2, 0>();
			auto vec = to_int_vector<1, 3>(casted);
			REQUIRE(is_equal(vec, {1, 2}));
		}

		TEST_CASE("reinterpret_cast counting down into negative") {
			integral_template_tuple_rev_v2<3, -3, Data> inTuple;
			// NOLINTNEXTLINE
			integral_template_tuple_rev_v2<2, -2, Data> &casted = inTuple.template subtuple<2, -2>();
			auto vec = to_int_vector<-1, 3>(casted);
			REQUIRE(is_equal(vec, {-1, 0, 1, 2}));
		}

		TEST_CASE("size") {
			REQUIRE(integral_template_tuple_rev_v2<1, -2, Data>::size() == 3);
			REQUIRE(integral_template_tuple_rev_v2<2, 1, Data>::size() == 1);
			REQUIRE(integral_template_tuple_rev_v2<3, 1, Data>::size() == 2);
		}

		TEST_CASE("visit") {
			integral_template_tuple_rev_v2<2, -1, Data> const tuple{};

			SUBCASE("non-void return") {
				auto const res = tuple.visit([acc = 0]<auto I>(Data<I> const &data) mutable {
					acc += static_cast<int>(data);
					return acc;
				});

				REQUIRE(res == 0 + 1 + 2);
			}

			SUBCASE("void return") {
				int acc = 0;
				tuple.visit([&acc]<auto I>(Data<I> const &data) {
					acc += static_cast<int>(data);
				});

				REQUIRE(acc == 0 + 1 + 2);
			}
		}
	}

}// namespace dice::template_library
