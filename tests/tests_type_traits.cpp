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

	TEST_CASE("copy_reference") {
		static_assert(std::is_same_v<copy_reference_t<int, double>, double>);
		static_assert(std::is_same_v<copy_reference_t<int &, double>, double &>);
		static_assert(std::is_same_v<copy_reference_t<int const &, double>, double &>);
		static_assert(std::is_same_v<copy_reference_t<int &&, double>, double &&>);
		static_assert(std::is_same_v<copy_reference_t<int const &&, double>, double &&>);
	}

	TEST_CASE("copy_cvref_t") {
		static_assert(std::is_same_v<copy_cvref_t<int, double>, double>);
		static_assert(std::is_same_v<copy_cvref_t<int const, double>, double const>);
		static_assert(std::is_same_v<copy_cvref_t<int volatile, double>, double volatile>);
		static_assert(std::is_same_v<copy_cvref_t<int const volatile, double>, double const volatile>);
		static_assert(std::is_same_v<copy_cvref_t<int &, double>, double &>);
		static_assert(std::is_same_v<copy_cvref_t<int const &, double>, double const &>);
		static_assert(std::is_same_v<copy_cvref_t<int volatile &, double>, double volatile &>);
		static_assert(std::is_same_v<copy_cvref_t<int const volatile &, double>, double const volatile &>);
		static_assert(std::is_same_v<copy_cvref_t<int &&, double>, double &&>);
		static_assert(std::is_same_v<copy_cvref_t<int const &&, double>, double const &&>);
		static_assert(std::is_same_v<copy_cvref_t<int volatile &&, double>, double volatile &&>);
		static_assert(std::is_same_v<copy_cvref_t<int const volatile &&, double>, double const volatile &&>);
	}

	struct forward_like_tester {
		int x;

		template<typename Self>
		decltype(auto) get(this Self &&self) {
			return forward_like<Self>(self.x);
		}
	};

	TEST_CASE("forward_like") {
		static_assert(std::is_same_v<decltype(std::declval<forward_like_tester const &>().get()), int const &>);
		static_assert(std::is_same_v<decltype(std::declval<forward_like_tester &>().get()), int &>);
		static_assert(std::is_same_v<decltype(std::declval<forward_like_tester &&>().get()), int &&>);
		static_assert(std::is_same_v<decltype(std::declval<forward_like_tester const &&>().get()), int const &&>);
	}

    TEST_CASE("move if value") {
	    int value = 0;
	    int &ref = value;
	    int const &cref = value;
	    int &&rref = std::move(value);
	    int const &&crref = std::move(value);

	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(value)), int &&>); // move
	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(ref)), int &>); // no move
	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(cref)), int const &>); // no move
	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(rref)), int &&>); // move
	    static_assert(std::is_same_v<decltype(DICE_MOVE_IF_VALUE(crref)), int const &&>); // no move
	}
}
