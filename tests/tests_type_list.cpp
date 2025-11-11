#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/type_list.hpp>

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <type_traits>
#include <vector>

TEST_SUITE("type_list") {
	namespace tl = dice::template_library::type_list;
	using empty_t = tl::type_list<>;

	TEST_CASE("first") {
		static_assert(std::is_same_v<tl::opt_t<tl::first<empty_t>>, tl::nullopt>);

		using t1 = tl::type_list<int>;
		static_assert(std::is_same_v<tl::first_t<t1>, int>);

		using t2 = tl::type_list<int, double>;
		static_assert(std::is_same_v<tl::first_t<t2>, int>);
	}

	TEST_CASE("last") {
		static_assert(std::is_same_v<tl::opt_t<tl::last<empty_t>>, tl::nullopt>);

		using t1 = tl::type_list<int>;
		static_assert(std::is_same_v<tl::last_t<t1>, int>);

		using t2 = tl::type_list<int, double>;
		static_assert(std::is_same_v<tl::last_t<t2>, double>);
	}

	TEST_CASE("nth") {
		static_assert(std::is_same_v<tl::opt_t<tl::nth<empty_t, 0>>, tl::nullopt>);
		static_assert(std::is_same_v<tl::opt_t<tl::nth<empty_t, 50>>, tl::nullopt>);


		using t1 = tl::type_list<int>;
		static_assert(std::is_same_v<tl::nth_t<t1, 0>, int>);
		static_assert(std::is_same_v<tl::opt_t<tl::nth<t1, 1>>, tl::nullopt>);

		using t2 = tl::type_list<int, double>;
		static_assert(std::is_same_v<tl::nth_t<t2, 0>, int>);
		static_assert(std::is_same_v<tl::nth_t<t2, 1>, double>);
		static_assert(std::is_same_v<tl::opt_t<tl::nth<t2, 2>>, tl::nullopt>);
	}

	TEST_CASE("concat") {
		static_assert(std::is_same_v<tl::concat_t<empty_t, empty_t>, empty_t>);

		using t1 = tl::type_list<int>;

		static_assert(std::is_same_v<tl::concat_t<empty_t, t1>, t1>);
		static_assert(std::is_same_v<tl::concat_t<t1, empty_t>, t1>);

		using t2 = tl::type_list<int, double>;

		static_assert(std::is_same_v<tl::concat_t<t1, t2>, tl::type_list<int, int, double>>);
		static_assert(std::is_same_v<tl::concat_t<t2, t1>, tl::type_list<int, double, int>>);
	}

	TEST_CASE("apply") {
		using t1 = tl::type_list<int>;
		static_assert(std::is_same_v<tl::apply_t<t1, std::vector>, std::vector<int>>);

		using t2 = tl::type_list<int, std::pmr::polymorphic_allocator<int>>;
		static_assert(std::is_same_v<tl::apply_t<t2, std::vector>, std::vector<int, std::pmr::polymorphic_allocator<int>>>);
	}

	TEST_CASE("transform") {
		constexpr auto func = []<typename T>(std::type_identity<T>) {
			return std::add_const<T>{};
		};

		static_assert(std::is_same_v<tl::transform_t<empty_t, func>, empty_t>);

		using t = tl::type_list<int, double, char>;
		static_assert(std::is_same_v<tl::transform_t<t, func>, tl::type_list<int const, double const, char const>>);
	}

	TEST_CASE("filter") {
		constexpr auto pred = []<typename T>(std::type_identity<T>) {
			return std::is_const_v<T>;
		};

		static_assert(std::is_same_v<tl::filter_t<empty_t, pred>, empty_t>);

		using t1 = tl::type_list<int, int const, double, char const>;
		static_assert(std::is_same_v<tl::filter_t<t1, pred>, tl::type_list<int const, char const>>);
	}

	TEST_CASE("generate") {
		static_assert(std::is_same_v<tl::generate_t<0, [] { }>, empty_t>);

		using t = tl::generate_t<3, []<size_t idx>(std::integral_constant<size_t, idx>) {
			if constexpr (idx == 0) {
				return std::type_identity<int>{};
			} else {
				return std::type_identity<double>{};
			}
		}>;

		static_assert(std::is_same_v<t, tl::type_list<int, double, double>>);
	}

	TEST_CASE("find_if") {
		constexpr auto pred1 = []<typename T>(std::type_identity<T>) {
			return std::is_const_v<T>;
		};

		constexpr auto pred2 = []<typename T>(std::type_identity<T>) {
			return std::is_same_v<T, int>;
		};

		constexpr auto pred3 = []<typename T>(std::type_identity<T>) {
			return std::is_same_v<T, double>;
		};

		constexpr auto pred4 = []<typename T>(std::type_identity<T>) {
			return false;
		};

		static_assert(std::is_same_v<tl::opt_t<tl::find_if<empty_t, pred1>>, tl::nullopt>);

		using t1 = tl::type_list<int, int const, double>;
		static_assert(std::is_same_v<tl::find_if_t<t1, pred1>, int const>);
		static_assert(std::is_same_v<tl::find_if_t<t1, pred2>, int>);
		static_assert(std::is_same_v<tl::find_if_t<t1, pred3>, double>);

		static_assert(std::is_same_v<tl::opt_t<tl::find_if<t1, pred4>>, tl::nullopt>);

		using t2 = tl::type_list<char, float>;
		static_assert(std::is_same_v<tl::opt_t<tl::find_if<t2, pred1>>, tl::nullopt>);
		static_assert(std::is_same_v<tl::opt_t<tl::find_if<t2, pred2>>, tl::nullopt>);
		static_assert(std::is_same_v<tl::opt_t<tl::find_if<t2, pred3>>, tl::nullopt>);
	}

	TEST_CASE("position") {
		constexpr auto pred1 = []<typename T>(std::type_identity<T>) {
			return std::is_const_v<T>;
		};

		constexpr auto pred2 = []<typename T>(std::type_identity<T>) {
			return std::is_same_v<T, int>;
		};

		constexpr auto pred3 = []<typename T>(std::type_identity<T>) {
			return std::is_same_v<T, double>;
		};

		constexpr auto pred4 = []<typename T>(std::type_identity<T>) {
			return false;
		};

		static_assert(tl::opt_v<tl::position<empty_t, pred1>> == tl::nullopt{});

		using t1 = tl::type_list<int, int const, double>;
		static_assert(tl::position_v<t1, pred1> == 1);
		static_assert(tl::position_v<t1, pred2> == 0);
		static_assert(tl::position_v<t1, pred3> == 2);

		static_assert(tl::opt_v<tl::position<t1, pred4>> == tl::nullopt{});

		using t2 = tl::type_list<char, float>;
		static_assert(tl::opt_v<tl::position<t2, pred1>> == tl::nullopt{});
		static_assert(tl::opt_v<tl::position<t2, pred2>> == tl::nullopt{});
		static_assert(tl::opt_v<tl::position<t2, pred3>> == tl::nullopt{});
	}

	TEST_CASE("contains") {
		static_assert(!tl::contains_v<empty_t, int>);

		using t1 = tl::type_list<int, double>;
		static_assert(tl::contains_v<t1, int>);
		static_assert(tl::contains_v<t1, double>);
		static_assert(!tl::contains_v<t1, char>);
	}

	TEST_CASE("all_of/any_of/none_of") {
		static_assert(tl::all_of_v<empty_t, [] {}>);
		static_assert(!tl::any_of_v<empty_t, [] {}>);
		static_assert(tl::none_of_v<empty_t, [] {}>);

		constexpr auto pred = []<typename T>(std::type_identity<T>) {
			return std::is_const_v<T>;
		};

		using t1 = tl::type_list<int const, double const>;
		static_assert(tl::all_of_v<t1, pred>);
		static_assert(tl::any_of_v<t1, pred>);
		static_assert(!tl::none_of_v<t1, pred>);

		using t2 = tl::type_list<int, double const>;
		static_assert(!tl::all_of_v<t2, pred>);
		static_assert(tl::any_of_v<t2, pred>);
		static_assert(!tl::none_of_v<t2, pred>);

		using t3 = tl::type_list<int, double>;
		static_assert(!tl::all_of_v<t3, pred>);
		static_assert(!tl::any_of_v<t3, pred>);
		static_assert(tl::none_of_v<t3, pred>);
	}

	TEST_CASE("all_same") {
		static_assert(tl::all_same_v<empty_t>);

		using t1 = tl::type_list<int, int>;
		static_assert(tl::all_same_v<t1>);

		using t2 = tl::type_list<int, double>;
		static_assert(!tl::all_same_v<t2>);
	}

	TEST_CASE("for_each") {
		using t1 = tl::type_list<int32_t, int64_t, int16_t>;

		std::vector<size_t> actual;
		auto func = [&]<typename T>(std::type_identity<T>) {
			actual.push_back(sizeof(T));
		};

		tl::for_each<empty_t>(func); // this should be a noop
		tl::for_each<t1>(func);

		CHECK_EQ(actual, std::vector<size_t>{4, 8, 2});
	}

	TEST_CASE("fold") {
		using t1 = tl::type_list<int32_t, int64_t, int16_t>;

		size_t actual = 0;
		auto func = [&]<typename T>(size_t acc, std::type_identity<T>) {
			return acc + sizeof(T);
		};

		actual += tl::fold<empty_t, size_t>(0, func); // this should be a noop
		actual += tl::fold<t1>(0UL, func);

		CHECK_EQ(actual, 4 + 8 + 2);
	}
}
