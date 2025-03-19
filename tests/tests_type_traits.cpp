#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/type_traits.hpp>
#include <type_traits>

TEST_SUITE("type_traits") {
	using namespace dice::template_library;

	TEST_CASE("is_zst") {
		struct zst {};
		struct non_zst {
			int x;
		};

		static_assert(is_zst<zst>::value);
		static_assert(is_zst_v<zst>);

		static_assert(!is_zst<non_zst>::value);
		static_assert(!is_zst_v<non_zst>);

		static_assert(!is_zst<void>::value);
		static_assert(!is_zst_v<void>);
	}

	TEST_CASE("copy_const") {
		static_assert(std::is_same_v<copy_const_t<int, double>, double>);
		static_assert(std::is_same_v<copy_const_t<int const, double>, double const>);
		static_assert(std::is_same_v<copy_const_t<int const &, double>, double>);
		static_assert(std::is_same_v<copy_const_t<int &, double>, double>);
		static_assert(std::is_same_v<copy_const_t<int volatile, double>, double>);
		static_assert(std::is_same_v<copy_const_t<int const, double const>, double const>);
		static_assert(std::is_same_v<copy_const_t<int const volatile, double const>, double const>);
	}

	TEST_CASE("copy_volatile") {
		static_assert(std::is_same_v<copy_volatile_t<int, double>, double>);
		static_assert(std::is_same_v<copy_volatile_t<int volatile, double>, double volatile>);
		static_assert(std::is_same_v<copy_volatile_t<int volatile &, double>, double>);
		static_assert(std::is_same_v<copy_volatile_t<int &, double>, double>);
		static_assert(std::is_same_v<copy_volatile_t<int const, double>, double>);
		static_assert(std::is_same_v<copy_volatile_t<int volatile, double volatile>, double volatile>);
		static_assert(std::is_same_v<copy_volatile_t<int const volatile, double volatile>, double volatile>);
	}

	TEST_CASE("copy_cv") {
		static_assert(std::is_same_v<copy_cv_t<int, double>, double>);
		static_assert(std::is_same_v<copy_cv_t<int const, double>, double const>);
		static_assert(std::is_same_v<copy_cv_t<int const &, double>, double>);
		static_assert(std::is_same_v<copy_cv_t<int &, double>, double>);
		static_assert(std::is_same_v<copy_cv_t<int, double>, double>);
		static_assert(std::is_same_v<copy_cv_t<int volatile, double>, double volatile>);
		static_assert(std::is_same_v<copy_cv_t<int volatile &, double>, double>);
		static_assert(std::is_same_v<copy_cv_t<int &, double>, double>);
		static_assert(std::is_same_v<copy_cv_t<int const volatile, double>, double const volatile>);
		static_assert(std::is_same_v<copy_cv_t<int const volatile, double const>, double const volatile>);
		static_assert(std::is_same_v<copy_cv_t<int const volatile, double volatile>, double const volatile>);
		static_assert(std::is_same_v<copy_cv_t<int const volatile, double const volatile>, double const volatile>);
	}
}
