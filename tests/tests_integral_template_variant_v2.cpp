#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <dice/template-library/integral_template_variant_v2.hpp>

#include <doctest/doctest.h>

#include <algorithm>

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
		REQUIRE(call_count == 1);
	}

	void call_counted(Data<1> &&) {
		static int call_count = 0;
		call_count += 1;
		REQUIRE(call_count == 1);
	}

	TEST_SUITE("integral_template_variant_v2") {
		TEST_CASE("asc pos") {
			(void) integral_template_variant_v2<2, 2, Data>{};
			integral_template_variant_v2<2, 8, Data> itv{Data<5>{}};
			integral_template_variant_v2<2, 8, Data> lower{Data<2>{}};
			integral_template_variant_v2<2, 8, Data> upper{Data<7>{}};

			REQUIRE(itv.index() == 5);
			REQUIRE(lower.index() == 2);
			REQUIRE(upper.index() == 7);

			REQUIRE(itv.template get<5>() == 5);
			REQUIRE(lower.template get<2>() == 2);
			REQUIRE(upper.template get<7>() == 7);

			itv.visit([]<auto N>(Data<N> const &) {
				REQUIRE(N == 5);
			});

			lower.visit([]<auto N>(Data<N> const &) {
				REQUIRE(N == 2);
			});

			upper.visit([]<auto N>(Data<N> const &) {
				REQUIRE(N == 7);
			});
		}

		TEST_CASE("asc neg") {
			integral_template_variant_v2<-6, 0, Data> itv{Data<-3>{}};
			integral_template_variant_v2<-6, 0, Data> lower{Data<-6>{}};
			integral_template_variant_v2<-6, 0, Data> upper{Data<-1>{}};

			REQUIRE(itv.index() == -3);
			REQUIRE(lower.index() == -6);
			REQUIRE(upper.index() == -1);

			REQUIRE(itv.template get<-3>() == -3);
			REQUIRE(lower.template get<-6>() == -6);
			REQUIRE(upper.template get<-1>() == -1);

			itv.visit([]<auto N>(Data<N> const &) {
				REQUIRE(N == -3);
			});

			lower.visit([]<auto N>(Data<N> const &) {
				REQUIRE(N == -6);
			});

			upper.visit([]<auto N>(Data<N> const &) {
				REQUIRE(N == -1);
			});
		}

		TEST_CASE("single entry") {
			integral_template_variant_v2<0, 1, Data> itv{Data<0>{}};
			REQUIRE(itv.template get<0>() == 0);
		}

		TEST_CASE("in place construction") {
			integral_template_variant_v2<0, 3, CompoundData> itv{std::in_place_type<CompoundData<1>>, 2.0, 3.f, 5};
			REQUIRE(itv.template get<1>() == 1 + 2 + 3 + 5);
		}

		TEST_CASE("visit forwarding") {
			integral_template_variant_v2<1, 2, Data> const copyable{std::in_place_type<Data<1>>};

			copyable.visit([]<typename T0>(T0 &&x) {
				call_counted(std::forward<T0>(x));
			});

			integral_template_variant_v2<1, 2, Data> movable{std::in_place_type<Data<1>>};

			std::move(movable).visit([]<typename T0>(T0 &&x) {
				call_counted(std::forward<T0>(x));
			});
		}
	}

	TEST_SUITE("integral_template_variant_v2 descending") {
		TEST_CASE("desc pos") {
			integral_template_variant_v2<7, 1, Data> itv{Data<5>{}};
			integral_template_variant_v2<7, 1, Data> upper{Data<7>{}};
			integral_template_variant_v2<7, 1, Data> lower{Data<2>{}};

			REQUIRE(itv.index() == 5);
			REQUIRE(upper.index() == 7);
			REQUIRE(lower.index() == 2);

			REQUIRE(itv.template get<5>() == 5);
			REQUIRE(upper.template get<7>() == 7);
			REQUIRE(lower.template get<2>() == 2);
		}

		TEST_CASE("desc neg") {
			integral_template_variant_v2<-2, -8, Data> itv{Data<-5>{}};
			integral_template_variant_v2<-2, -8, Data> upper{Data<-2>{}};
			integral_template_variant_v2<-2, -8, Data> lower{Data<-7>{}};

			REQUIRE(itv.index() == -5);
			REQUIRE(upper.index() == -2);
			REQUIRE(lower.index() == -7);

			REQUIRE(itv.template get<-5>() == -5);
			REQUIRE(upper.template get<-2>() == -2);
			REQUIRE(lower.template get<-7>() == -7);
		}

		TEST_CASE("emplace") {
			integral_template_variant_v2<10, 5, CompoundData> itv{std::in_place_type<CompoundData<7>>, 1.0, 2.0f, 3};
			REQUIRE(itv.index() == 7);
			REQUIRE(itv.template get<7>() == 13);

			itv.template emplace<9>(2.0, 3.0f, 4);
			REQUIRE(itv.index() == 9);
			REQUIRE(itv.template get<9>() == 18);
		}

		TEST_CASE("visit") {
			integral_template_variant_v2<10, 5, Data> itv{Data<7>{}};

			int result = itv.visit([]<typename T>(T &&x) -> int {
				return std::forward<T>(x);
			});

			REQUIRE(result == 7);
		}

		TEST_CASE("holds_alternative") {
			integral_template_variant_v2<10, 5, Data> itv{Data<7>{}};

			REQUIRE(holds_alternative<7>(itv));
			REQUIRE_FALSE(holds_alternative<6>(itv));
			REQUIRE_FALSE(holds_alternative<10>(itv));

			REQUIRE(holds_alternative<Data<7>>(itv));
			REQUIRE_FALSE(holds_alternative<Data<6>>(itv));
		}

		TEST_CASE("comparison") {
			integral_template_variant_v2<10, 5, Data> itv1{Data<7>{}};
			integral_template_variant_v2<10, 5, Data> itv2{Data<7>{}};
			integral_template_variant_v2<10, 5, Data> itv3{Data<8>{}};

			REQUIRE(itv1 == itv2);
			REQUIRE(itv1 != itv3);
		}
	}

}// namespace dice::template_library
