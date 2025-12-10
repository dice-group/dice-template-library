#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <type_traits>

#include <dice/template-library/standard_layout_tuple.hpp>
#include <dice/template-library/tuple_algorithm.hpp>

TEST_SUITE("standard layout tuple") {
	using namespace dice::template_library;

	TEST_CASE("standard layout tuple") {
		struct example_struct {
			int a;
			bool b;
			float f;
		};
		static_assert(std::is_standard_layout_v<example_struct>);

		using example_tuple = standard_layout_tuple<int,
													bool,
													float>;
		static_assert(std::is_standard_layout_v<example_tuple>);

		example_tuple example_tuple_v{};
		example_struct example_struct_v{};

		auto const offset = [](void const *base, void const *member) -> size_t {
			return static_cast<std::byte const *>(base) - static_cast<std::byte const *>(member);
		};

		// best effort correctness detection
		static_assert((sizeof(example_struct) == sizeof(example_tuple)));
		REQUIRE((offset(&example_struct_v, &example_struct_v.a) == offset(&example_tuple_v, &example_tuple_v.get<0>())));
		REQUIRE((offset(&example_struct_v, &example_struct_v.b) == offset(&example_tuple_v, &example_tuple_v.get<1>())));
		REQUIRE((offset(&example_struct_v, &example_struct_v.f) == offset(&example_tuple_v, &example_tuple_v.get<2>())));

		REQUIRE((offset(&example_struct_v, &example_struct_v.a) == offset(&example_tuple_v, &example_tuple_v.get<int>())));
		REQUIRE((offset(&example_struct_v, &example_struct_v.b) == offset(&example_tuple_v, &example_tuple_v.get<bool>())));
		REQUIRE((offset(&example_struct_v, &example_struct_v.f) == offset(&example_tuple_v, &example_tuple_v.get<float>())));
	}

	TEST_CASE("non-standard-layout-tuple") {
		using tuple = standard_layout_tuple<std::tuple<int, int, int>>;
		static_assert(!std::is_standard_layout_v<tuple>);
	}

	TEST_CASE("ctors") {
		standard_layout_tuple<int, double, float> tup{1, 42.0, 120.0f};

		auto check = [](auto const &x) {
			REQUIRE_EQ(x.template get<0>(), 1);
			REQUIRE_EQ(x.template get<int>(), 1);
			REQUIRE_EQ(x.template get<1>(), 42.0);
			REQUIRE_EQ(x.template get<double>(), 42.0);
			REQUIRE_EQ(x.template get<2>(), 120.0f);
			REQUIRE_EQ(x.template get<float>(), 120.0f);
		};

		check(tup);

		auto copy = tup;
		check(copy);
		check(tup);

		auto move = std::move(tup);
		check(move);
		check(tup);

		move = copy;
		check(move);
		check(copy);

		tup = std::move(copy);
		check(tup);
		check(move);
	}

	TEST_CASE("size") {
		REQUIRE_EQ(standard_layout_tuple<>::size(), 0);
		REQUIRE_EQ(std::tuple_size_v<standard_layout_tuple<>>, 0);

		REQUIRE_EQ(standard_layout_tuple<int>::size(), 1);
		REQUIRE_EQ(std::tuple_size_v<standard_layout_tuple<int>>, 1);

		REQUIRE_EQ(standard_layout_tuple<int, double>::size(), 2);
		REQUIRE_EQ(std::tuple_size_v<standard_layout_tuple<int, double>>, 2);
	}

	TEST_CASE("destructuring") {
		standard_layout_tuple<int, double> tup{42, 123.0};

		auto [x, y] = tup;
		REQUIRE_EQ(x, 42);
		REQUIRE_EQ(y, 123.0);
	}

	TEST_CASE("comparison") {
		standard_layout_tuple<int, int> const a{1, 1};
		standard_layout_tuple<int, int> const b{1, 2};
		standard_layout_tuple<int, int> const c{2, 1};

		REQUIRE_EQ(a, a);
		REQUIRE_EQ(b, b);
		REQUIRE_EQ(c, c);

		REQUIRE_LT(a, b);
		REQUIRE_LT(b, c);
		REQUIRE_LT(a, c);
	}

	TEST_CASE("subtuple") {
		standard_layout_tuple<int, double, float> tup{1, 123.0, 42.0f};

		SUBCASE("resulting type") {
			auto &&ref = tup.template subtuple<0, 0>();
			static_assert(std::is_same_v<decltype(ref), standard_layout_tuple<> &>);

			auto &&const_ref = std::as_const(tup).template subtuple<0, 0>();
			static_assert(std::is_same_v<decltype(const_ref), standard_layout_tuple<> const &>);

			auto &&rref = std::move(tup).template subtuple<0, 0>();
			static_assert(std::is_same_v<decltype(rref), standard_layout_tuple<> &&>);

			auto &&const_rref = static_cast<decltype(tup) const &&>(tup).template subtuple<0, 0>();
			static_assert(std::is_same_v<decltype(const_rref), standard_layout_tuple<> const &&>);
		}

		SUBCASE("resulting value") {
			auto const &empty = tup.template subtuple<0, 0>();
			REQUIRE_EQ(empty.size(), 0);

			auto const &noop = tup.template subtuple<0, 3>();
			REQUIRE_EQ(noop.size(), 3);
			REQUIRE_EQ(noop, tup);

			auto const &s1 = tup.template subtuple<0, 1>();
			REQUIRE_EQ(s1.size(), 1);
			REQUIRE_EQ(s1.template get<0>(), 1);

			auto const &s2 = tup.template subtuple<0, 2>();
			REQUIRE_EQ(s2.size(), 2);
			REQUIRE_EQ(s2.template get<0>(), 1);
			REQUIRE_EQ(s2.template get<1>(), 123.0);

			auto const &s3 = tup.template subtuple<1, 1>();
			REQUIRE_EQ(s3.size(), 1);
			REQUIRE_EQ(s3.template get<0>(), 123.0);

			auto const &s4 = tup.template subtuple<1, 2>();
			REQUIRE_EQ(s4.size(), 2);
			REQUIRE_EQ(s4.template get<0>(), 123.0);
			REQUIRE_EQ(s4.template get<1>(), 42.0f);
		}
	}

	TEST_CASE("tuple algorithms") {
		standard_layout_tuple<int, double, float> const tuple{1, 2.0, 3.0f};

		SUBCASE("fold") {
			auto const res = tuple_fold(tuple, 0.0, [](double acc, auto elem) {
				return acc + elem;
			});

			REQUIRE(res == 6.0);
		}

		SUBCASE("void return") {
			double acc = 0;
			tuple_for_each(tuple, [&acc](auto elem) {
				acc += elem;
			});

			REQUIRE(acc == 6.0);
		}
	}
}
