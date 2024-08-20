#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/variant2.hpp>

TEST_SUITE("variant2") {
	using namespace dice::template_library;

	template<typename T, typename Variant>
	struct variant_index;

	template<typename T, typename U>
	struct variant_index<T, variant2<T, U>> {
		static constexpr size_t value = 0;
	};

	template<typename T, typename U>
	struct variant_index<T, variant2<U, T>> {
		static constexpr size_t value = 1;
	};

	template<typename T, typename U>
	static constexpr bool same_kind_of_type = std::is_lvalue_reference_v<T> == std::is_lvalue_reference_v<U>
		&& std::is_rvalue_reference_v<T> == std::is_rvalue_reference_v<U>
		&& std::is_const_v<std::remove_reference_t<T>> == std::is_const_v<std::remove_reference_t<U>>;


	struct make_valueless {
		make_valueless() {
			throw std::runtime_error{"aaa"};
		}

		explicit make_valueless(std::nothrow_t) noexcept {}

		make_valueless(make_valueless const &) {
			throw std::runtime_error{"aaa"};
		}

		make_valueless(make_valueless &&) {
			throw std::runtime_error{"aaa"};
		}

		make_valueless &operator=(make_valueless const &) {
			throw std::runtime_error{"aaa"};
		}

		make_valueless &operator=(make_valueless &&) {
			throw std::runtime_error{"aaa"};
		}

		~make_valueless() noexcept = default;

		auto operator<=>(make_valueless const &) const noexcept = default;
	};

	template<typename T, typename Variant>
	void check_acessors(Variant &&x, T const &expected_val) {
		static constexpr size_t index = variant_index<T, std::remove_cvref_t<Variant>>::value;

		static_assert(same_kind_of_type<decltype(get<T>(std::forward<Variant>(x))), decltype(std::forward<Variant>(x))>);
		CHECK_EQ(get<T>(std::forward<Variant>(x)), expected_val);

		if constexpr (std::is_lvalue_reference_v<decltype(std::forward<Variant>(x))>) {
			static_assert(same_kind_of_type<decltype(get_if<T>(&std::forward<Variant>(x))), decltype(&std::forward<Variant>(x))>);
			CHECK_EQ(*get_if<T>(&std::forward<Variant>(x)), expected_val);
		}

		CHECK(holds_alternative<T>(x));
		CHECK_FALSE(x.valueless_by_exception());
		CHECK_EQ(x.index(), index);

		visit([&]<typename U>(U &&val) {
			static_assert(same_kind_of_type<decltype(std::forward<U>(val)), decltype(std::forward<Variant>(x))>);

			if constexpr (std::is_same_v<std::remove_cvref_t<U>, T>) {
				CHECK_EQ(val, expected_val);
			} else {
				FAIL("Unexpected element active");
			}
		}, std::forward<Variant>(x));

		std::remove_cvref_t<Variant> const y{std::in_place_type<T>, expected_val};

		CHECK(x == x);
		CHECK_FALSE(x != x);
		CHECK(x <=> x == std::strong_ordering::equal);

		CHECK(x == y);
		CHECK_FALSE(x != y);
		CHECK(x <=> y == std::strong_ordering::equal);

		if constexpr (requires { std::hash<std::variant_alternative_t<0, std::remove_cvref_t<Variant>>>{}; } && requires { std::hash<std::variant_alternative_t<1, std::remove_cvref_t<Variant>>>{}; }) {
			[[maybe_unused]] auto h = std::hash<std::remove_cvref_t<Variant>>{}(x); // only checking if this compiles
		}
	}

	template<typename Variant>
	void check_valuelessness(Variant const &v) {
		CHECK(v.valueless_by_exception());
		CHECK_EQ(v.index(), std::variant_npos);
		CHECK_THROWS((void) get<int>(v));
		CHECK_THROWS((void) get<make_valueless>(v));
		CHECK_EQ(get_if<int>(&v), nullptr);
		CHECK_EQ(get_if<make_valueless>(&v), nullptr);
		CHECK_THROWS((void) visit([](auto &&x) { }, v));
	}

	TEST_CASE("properties") {
		static_assert(std::is_same_v<std::variant_alternative_t<0, variant2<int, double>>, int>);
		static_assert(std::is_same_v<std::variant_alternative_t<1, variant2<int, double>>, double>);
		static_assert(std::variant_size_v<variant2<int, double>> == 2);
	}

	TEST_CASE("value categories") {
		variant2<int, double> const const_ref{};
		check_acessors(const_ref, 0);

		variant2<int, double> ref{};
		check_acessors(ref, 0);

		variant2<int, double> xref{};
		check_acessors(std::move(xref), 0);

		variant2<int, double> const const_xref{};
		check_acessors(std::move(const_xref), 0);
	}

	TEST_CASE("ctors") {
		variant2<int, double> const a{std::in_place_type<int>, 5};
		check_acessors(a, 5);

		variant2<int, double> const b{std::in_place_type<double>, 6.0};
		check_acessors(b, 6.0);

		variant2<int, double> const c{std::in_place_index<0>, 5};
		check_acessors(c, 5);

		variant2<int, double> const d{std::in_place_index<1>, 6.0};
		check_acessors(d, 6.0);

		variant2<int, double> const e{5};
		check_acessors(e, 5);

		variant2<int, double> const f{6.0};
		check_acessors(f, 6.0);

		variant2<int, double> const g{};
		check_acessors(g, 0);
	}

	TEST_CASE("assignment operators") {
		variant2<int, double> x{std::in_place_type<int>, 5};
		check_acessors(x, 5);

		int const a = 6;
		x = a;
		check_acessors(x, 6);
		x = a;
		check_acessors(x, 6);

		double const b = 7.0;
		x = b;
		check_acessors(x, 7.0);
		x = b;
		check_acessors(x, 7.0);

		int c = 8;
		x = std::move(c);
		check_acessors(x, 8);
		x = std::move(c);
		check_acessors(x, 8);

		double d = 9.0;
		x = std::move(d);
		check_acessors(x, 9.0);
		x = std::move(d);
		check_acessors(x, 9.0);

		variant2<int, double> const e{10};
		x = e;
		check_acessors(x, 10);
		x = e;
		check_acessors(x, 10);

		variant2<int, double> f{11.0};
		x = f;
		check_acessors(x, 11.0);
		x = std::move(f);
		check_acessors(x, 11.0);

		x = x;
		check_acessors(x, 11.0);
	}

	TEST_CASE("valueless by exception") {
		variant2<int, make_valueless> v{};
		check_acessors(v, 0);

		CHECK_THROWS(v.emplace<make_valueless>());
		check_valuelessness(v);

		int const a = 5;
		v = a;
		check_acessors(v, 5);

		CHECK_THROWS(v = make_valueless{std::nothrow});
		check_valuelessness(v);

		int b = 6;
		v = std::move(b);
		check_acessors(v, b);

		make_valueless const novalue{std::nothrow};
		CHECK_THROWS(v = novalue);
		check_valuelessness(v);

		variant2<int, make_valueless> const c{8};
		v = c;
		check_acessors(v, 8);

		make_valueless novalue2{std::nothrow};
		CHECK_THROWS(v = std::move(novalue2));
		check_valuelessness(v);

		variant2<int, make_valueless> d{9};
		v = std::move(d);
		check_acessors(v, 9);

		CHECK_THROWS(v.emplace<make_valueless>(make_valueless{std::nothrow}));
		check_valuelessness(v);

		variant2<int, make_valueless> const e{std::in_place_type<make_valueless>, std::nothrow};
		CHECK_THROWS(v = e);
		check_valuelessness(v);

		variant2<int, make_valueless> f{std::in_place_type<make_valueless>, std::nothrow};
		CHECK_THROWS(v = std::move(f));
		check_valuelessness(v);
	}

	TEST_CASE("variant typedef") {
		static_assert(std::is_same_v<variant<int, double>, variant2<int, double>>);
		static_assert(std::is_same_v<variant<>, std::variant<>>);
		static_assert(std::is_same_v<variant<int>, std::variant<int>>);
		static_assert(std::is_same_v<variant<int, double, float>, std::variant<int, double, float>>);
	}
}
