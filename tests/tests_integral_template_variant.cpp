#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/integral_template_variant.hpp>

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

	TEST_SUITE("integral_template_variant") {
		TEST_CASE("asc pos") {
			integral_template_variant<2, 7, Data> itv{Data<5>{}};
			integral_template_variant<2, 7, Data> lower{Data<2>{}};
			integral_template_variant<2, 7, Data> upper{Data<7>{}};

			CHECK(itv.index() == 5);
			CHECK(lower.index() == 2);
			CHECK(upper.index() == 7);

			CHECK(itv.template get<5>() == 5);
			CHECK(lower.template get<2>() == 2);
			CHECK(upper.template get<7>() == 7);

			itv.visit([]<auto N>(Data<N> const &) {
				CHECK(N == 5);
			});

			lower.visit([]<auto N>(Data<N> const &) {
				CHECK(N == 2);
			});

			upper.visit([]<auto N>(Data<N> const &) {
				CHECK(N == 7);
			});
		}

		TEST_CASE("desc pos") {
			integral_template_variant<5, 1, Data> itv{Data<3>{}};
			integral_template_variant<5, 1, Data> lower{Data<5>{}};
			integral_template_variant<5, 1, Data> upper{Data<1>{}};

			CHECK(itv.index() == 3);
			CHECK(lower.index() == 5);
			CHECK(upper.index() == 1);

			CHECK(itv.template get<3>() == 3);
			CHECK(lower.template get<5>() == 5);
			CHECK(upper.template get<1>() == 1);

			itv.visit([]<auto N>(Data<N> const &) {
				CHECK(N == 3);
			});

			lower.visit([]<auto N>(Data<N> const &) {
				CHECK(N == 5);
			});

			upper.visit([]<auto N>(Data<N> const &) {
				CHECK(N == 1);
			});
		}

		TEST_CASE("asc neg") {
			integral_template_variant<-6, -1, Data> itv{Data<-3>{}};
			integral_template_variant<-6, -1, Data> lower{Data<-6>{}};
			integral_template_variant<-6, -1, Data> upper{Data<-1>{}};

			CHECK(itv.index() == -3);
			CHECK(lower.index() == -6);
			CHECK(upper.index() == -1);

			CHECK(itv.template get<-3>() == -3);
			CHECK(lower.template get<-6>() == -6);
			CHECK(upper.template get<-1>() == -1);

			itv.visit([]<auto N>(Data<N> const &) {
				CHECK(N == -3);
			});

			lower.visit([]<auto N>(Data<N> const &) {
				CHECK(N == -6);
			});

			upper.visit([]<auto N>(Data<N> const &) {
				CHECK(N == -1);
			});
		}

		TEST_CASE("desc pos") {
			integral_template_variant<-2, -8, Data> itv{Data<-5>{}};
			integral_template_variant<-2, -8, Data> lower{Data<-2>{}};
			integral_template_variant<-2, -8, Data> upper{Data<-8>{}};

			CHECK(itv.index() == -5);
			CHECK(lower.index() == -2);
			CHECK(upper.index() == -8);

			CHECK(itv.template get<-5>() == -5);
			CHECK(lower.template get<-2>() == -2);
			CHECK(upper.template get<-8>() == -8);

			itv.visit([]<auto N>(Data<N> const &) {
				CHECK(N == -5);
			});

			lower.visit([]<auto N>(Data<N> const &) {
				CHECK(N == -2);
			});

			upper.visit([]<auto N>(Data<N> const &) {
				CHECK(N == -8);
			});
		}

		TEST_CASE("single entry") {
			integral_template_variant<0, 0, Data> itv{Data<0>{}};
			CHECK(itv.template get<0>() == 0);
		}

		TEST_CASE("in place construction") {
			integral_template_variant<0, 2, CompoundData> itv{std::in_place_type<CompoundData<1>>, 2.0, 3.f, 5};
			CHECK(itv.template get<1>() == 1 + 2 + 3 + 5);
		}

		TEST_CASE("visit forwarding") {
			integral_template_variant<1, 1, Data> const copyable{std::in_place_type<Data<1>>};

			copyable.visit([](auto &&x) {
				call_counted(std::forward<decltype(x)>(x));
			});

			integral_template_variant<1, 1, Data> movable{std::in_place_type<Data<1>>};

			std::move(movable).visit([](auto &&x) {
				call_counted(std::forward<decltype(x)>(x));
			});
		}
	}

} // dice::template_library
