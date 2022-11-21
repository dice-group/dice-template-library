#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/integral_templated_variant.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <iostream>

namespace dice::template_library {

	template<auto N>
	struct Data {
		operator int() const { return static_cast<int>(N); }
	};

	template<auto N>
	struct CompoundData {
		double d;
		float f;
		int a;

		CompoundData(double d, float f, int a) : d{d}, f{f}, a{a} {}

		operator int() const { return static_cast<int>(d + f + a + N); }
	};

	void call_counted(Data<1> const &) {
		static int call_count = 0;
		call_count += 1;
		CHECK(call_count == 1);
	}

	void call_counted(Data<1> &&) {
		static int call_count = 0;
		call_count += 1;
		CHECK(call_count == 1);
	}

	TEST_SUITE("integral_templated_variant") {
		TEST_CASE("asc pos") {
			integral_templated_variant<Data, 2, 7> itv{Data<5>{}};
			integral_templated_variant<Data, 2, 7> lower{Data<2>{}};
			integral_templated_variant<Data, 2, 7> upper{Data<7>{}};

			CHECK(get<5>(itv) == 5);
			CHECK(get<2>(lower) == 2);
			CHECK(get<7>(upper) == 7);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == 5);
			}, itv);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == 2);
			}, lower);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == 7);
			}, upper);
		}

		TEST_CASE("desc pos") {
			integral_templated_variant<Data, 5, 1> itv{Data<3>{}};
			integral_templated_variant<Data, 5, 1> lower{Data<5>{}};
			integral_templated_variant<Data, 5, 1> upper{Data<1>{}};

			CHECK(get<3>(itv) == 3);
			CHECK(get<5>(lower) == 5);
			CHECK(get<1>(upper) == 1);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == 3);
			}, itv);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == 5);
			}, lower);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == 1);
			}, upper);
		}

		TEST_CASE("asc neg") {
			integral_templated_variant<Data, -6, -1> itv{Data<-3>{}};
			integral_templated_variant<Data, -6, -1> lower{Data<-6>{}};
			integral_templated_variant<Data, -6, -1> upper{Data<-1>{}};

			CHECK(get<-3>(itv) == -3);
			CHECK(get<-6>(lower) == -6);
			CHECK(get<-1>(upper) == -1);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == -3);
			}, itv);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == -6);
			}, lower);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == -1);
			}, upper);
		}

		TEST_CASE("desc pos") {
			integral_templated_variant<Data, -2, -8> itv{Data<-5>{}};
			integral_templated_variant<Data, -2, -8> lower{Data<-2>{}};
			integral_templated_variant<Data, -2, -8> upper{Data<-8>{}};

			CHECK(get<-5>(itv) == -5);
			CHECK(get<-2>(lower) == -2);
			CHECK(get<-8>(upper) == -8);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == -5);
			}, itv);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == -2);
			}, lower);

			visit([]<auto N>(Data<N> const &) {
				CHECK(N == -8);
			}, upper);
		}

		TEST_CASE("single entry") {
			integral_templated_variant<Data, 0, 0> itv{Data<0>{}};
			CHECK(get<0>(itv) == 0);
		}

		TEST_CASE("in place construction") {
			integral_templated_variant<CompoundData, 0, 2> itv{std::in_place_type<CompoundData<1>>, 2.0, 3.f, 5};
			CHECK(get<1>(itv) == 1 + 2 + 3 + 5);
		}

		TEST_CASE("visit forwarding") {
			integral_templated_variant<Data, 1, 1> const copyable{std::in_place_type<Data<1>>};

			visit([](auto &&x) {
				call_counted(std::forward<decltype(x)>(x));
			}, copyable);

			integral_templated_variant<Data, 1, 1> movable{std::in_place_type<Data<1>>};

			visit([](auto &&x) {
				call_counted(std::forward<decltype(x)>(x));
			}, std::move(movable));
		}
	}

} // dice::template_library
