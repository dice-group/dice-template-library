#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/mutex.hpp>
#include <type_traits>

TEST_SUITE("mutex") {
	using namespace dice::template_library;

	static_assert(!std::is_copy_constructible_v<mutex<int>>);
	static_assert(!std::is_move_constructible_v<mutex<int>>);
	static_assert(!std::is_copy_assignable_v<mutex<int>>);
	static_assert(!std::is_move_assignable_v<mutex<int>>);

	TEST_CASE("defalt ctor") {
		mutex<int> mut{};
		REQUIRE_EQ(*mut.lock(), 0);
	}

	TEST_CASE("copy value inside") {
		std::vector<int> vec{1, 2, 3};
		mutex<std::vector<int>> mut{vec};
		REQUIRE_EQ(mut.lock()->size(), 3);
	}

	TEST_CASE("move value inside") {
		std::vector<int> vec{1, 2, 3};
		mutex<std::vector<int>> mut{std::move(vec)};
		REQUIRE_EQ(mut.lock()->size(), 3);
	}

	TEST_CASE("in place construction") {
		struct data {
			int x;
			double y;
		};

		mutex<data> mut{std::in_place, 5, 12.2};
		REQUIRE_EQ(mut.lock()->x, 5);
		REQUIRE_EQ(mut.lock()->y, 12.2);
	}

	TEST_CASE("swap") {
		mutex<int> a{1};
		mutex<int> b{2};

		auto ga = a.lock();
		auto gb = b.lock();

		REQUIRE_EQ(*ga, 1);
		REQUIRE_EQ(*gb, 2);

		swap(ga, gb);

		REQUIRE_EQ(*ga, 2);
		REQUIRE_EQ(*gb, 1);
	}

	TEST_CASE("lock compiles") {
		mutex<int> mut{};
		REQUIRE_EQ(*mut.lock(), 0);
	}

	TEST_CASE("try_lock compiles") {
		mutex<int> mut{};
		auto guard = mut.try_lock();
		REQUIRE(guard.has_value());
		REQUIRE_EQ(**guard, 0);
	}
}
