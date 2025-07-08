#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/static_string.hpp>

#include <ranges>

TEST_SUITE("static_string") {
	using namespace dice::template_library;

	template<typename T>
	struct weird_allocator : std::allocator<T> {
		using is_always_equal = std::false_type;
		using propagate_on_container_move_assignment = std::false_type;

		bool operator==([[maybe_unused]] weird_allocator const &other) const noexcept {
			return false;
		}
	};

	using weird_static_string = basic_static_string<char, std::char_traits<char>, weird_allocator<char>>;

	static_assert(sizeof(static_string) == 2 * sizeof(void *));

	template<typename Allocator>
	void check_empty_string(basic_static_string<char, std::char_traits<char>, Allocator> const &s) {
		REQUIRE_EQ(s.data(), nullptr);
		REQUIRE_EQ(s.size(), 0);
		REQUIRE(s.empty());
		REQUIRE_EQ(s.begin(), s.end());
		REQUIRE_EQ(s.rbegin(), s.rend());
		REQUIRE_EQ(s.cbegin(), s.cend());
		REQUIRE_EQ(s.crbegin(), s.crend());
		REQUIRE_EQ(s, "");
		REQUIRE_EQ(static_cast<std::string_view>(s), "");
	}

	template<typename Allocator>
	void check_non_empty_string(basic_static_string<char, std::char_traits<char>, Allocator> const &actual, std::string_view expected) {
		assert(!expected.empty());

		REQUIRE_EQ(actual.size(), expected.size());
		REQUIRE_EQ(actual, actual);
		REQUIRE_EQ(actual, expected);
		REQUIRE_EQ(actual <=> expected, std::strong_ordering::equal);
		REQUIRE_EQ(actual <=> actual, std::strong_ordering::equal);
		REQUIRE_FALSE(actual.empty());
		REQUIRE(std::ranges::equal(actual, expected));
		REQUIRE(std::ranges::equal(actual | std::views::reverse, expected | std::views::reverse));
		REQUIRE_EQ(std::memcmp(actual.data(), expected.data(), actual.size()), 0);
		REQUIRE_EQ(actual.front(), expected.front());
		REQUIRE_EQ(actual.back(), expected.back());
		REQUIRE_EQ(actual[0], expected[0]);
		REQUIRE_EQ(static_cast<std::string_view>(actual), expected);
	}

	TEST_CASE("empty string") {
		SUBCASE("default ctor") {
			static_string const s;
			check_empty_string(s);
		}

		SUBCASE("string view ctor") {
			static_string const s{""};
			check_empty_string(s);
		}
	}

	TEST_CASE("move ctor") {
		std::string_view const expected = "Hello World";
		static_string const s{expected};
		check_non_empty_string(s, expected);

		static_string const s2{std::move(s)};

		check_empty_string(s);
		check_non_empty_string(s2, expected);
	}

	TEST_CASE("copy ctor") {
		std::string_view const expected = "Hello World";

		static_string const s{expected};
		check_non_empty_string(s, expected);

		static_string const s2{s};
		check_non_empty_string(s, expected);
		check_non_empty_string(s2, expected);
	}

	TEST_CASE("move assignment") {
		SUBCASE("regular") {
			SUBCASE("std allocator") {
				static_string s1{"Hello World"};
				static_string s2{"Spherical Cow"};
				s1 = std::move(s2);
				check_non_empty_string(s1, "Spherical Cow");
			}

			SUBCASE("weird allocator") {
				weird_static_string s1{"Hello World"};
				weird_static_string s2{"Spherical Cow"};

				s1 = std::move(s2);
				check_non_empty_string(s1, "Spherical Cow");
			}
		}

		SUBCASE("assign empty") {
			SUBCASE("std alloctor") {
				static_string empty;
				check_empty_string(empty);

				static_string s{"Hello World"};
				check_non_empty_string(s, "Hello World");

				s = std::move(empty);
				check_empty_string(s);
			}

			SUBCASE("weird allocator") {
				weird_static_string empty;
				check_empty_string(empty);

				weird_static_string s{"Hello World"};
				check_non_empty_string(s, "Hello World");

				s = std::move(empty);
				check_empty_string(s);
			}
		}
	}

	TEST_CASE("copy assignment") {
		SUBCASE("regular") {
			SUBCASE("std allocator") {
				static_string s1{"Hello World"};
				static_string const s2{"Spherical Cow"};
				s1 = s2;
				check_non_empty_string(s1, "Spherical Cow");
				check_non_empty_string(s2, "Spherical Cow");
			}

			SUBCASE("weird allocator") {
				weird_static_string s1{"Hello World"};
				weird_static_string const s2{"Spherical Cow"};

				s1 = s2;
				check_non_empty_string(s1, "Spherical Cow");
				check_non_empty_string(s2, "Spherical Cow");
			}
		}

		SUBCASE("assign empty") {
			SUBCASE("std alloctor") {
				static_string const empty;
				check_empty_string(empty);

				static_string s{"Hello World"};
				check_non_empty_string(s, "Hello World");

				s = empty;
				check_empty_string(s);
				check_empty_string(empty);
			}

			SUBCASE("weird allocator") {
				weird_static_string const empty;
				check_empty_string(empty);

				weird_static_string s{"Hello World"};
				check_non_empty_string(s, "Hello World");

				s = empty;
				check_empty_string(s);
				check_empty_string(empty);
			}
		}
	}

	TEST_CASE("swap") {
		std::string_view const expected1 = "Hello World";
		std::string_view const expected2 = "Spherical Cow";

		static_string s1{expected1};
		check_non_empty_string(s1, expected1);

		static_string s2{expected2};
		check_non_empty_string(s2, expected2);

		swap(s1, s2);
		check_non_empty_string(s1, expected2);
		check_non_empty_string(s2, expected1);
	}
}
