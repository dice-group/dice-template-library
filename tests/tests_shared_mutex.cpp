#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/shared_mutex.hpp>
#include <type_traits>

TEST_SUITE("shared_mutex") {
	using namespace dice::template_library;

	static_assert(!std::is_copy_constructible_v<shared_mutex<int>>);
	static_assert(!std::is_move_constructible_v<shared_mutex<int>>);
	static_assert(!std::is_copy_assignable_v<shared_mutex<int>>);
	static_assert(!std::is_move_assignable_v<shared_mutex<int>>);

	TEST_CASE("defalt ctor") {
		shared_mutex<int> mut{};
		REQUIRE_EQ(*mut.lock(), 0);
	}

	TEST_CASE("copy value inside") {
		std::vector<int> vec{1, 2, 3};
		shared_mutex<std::vector<int>> mut{vec};
		REQUIRE_EQ(mut.lock()->size(), 3);
	}

	TEST_CASE("move value inside") {
		std::vector<int> vec{1, 2, 3};
		shared_mutex<std::vector<int>> mut{std::move(vec)};
		REQUIRE_EQ(mut.lock()->size(), 3);
	}

	TEST_CASE("in place construction") {
		struct data {
			int x;
			double y;
		};

		shared_mutex<data> mut{std::in_place, 5, 12.2};
		REQUIRE_EQ(mut.lock()->x, 5);
		REQUIRE_EQ(mut.lock()->y, 12.2);
	}

	TEST_CASE("swap") {
		shared_mutex<int> a{1};
		shared_mutex<int> b{2};

		SUBCASE("lock") {
			auto ga = a.lock();
			auto gb = b.lock();

			REQUIRE_EQ(*ga, 1);
			REQUIRE_EQ(*gb, 2);

			swap(ga, gb);

			REQUIRE_EQ(*ga, 2);
			REQUIRE_EQ(*gb, 1);
		}

		SUBCASE("lock_shared") {
			auto ga = a.lock_shared();
			auto gb = b.lock_shared();

			REQUIRE_EQ(*ga, 1);
			REQUIRE_EQ(*gb, 2);

			swap(ga, gb);

			REQUIRE_EQ(*ga, 2);
			REQUIRE_EQ(*gb, 1);
		}
	}

	TEST_CASE("lock compiles") {
		shared_mutex<int> mut{};
		REQUIRE_EQ(*mut.lock(), 0);
	}

	TEST_CASE("try_lock compiles") {
		shared_mutex<int> mut{};
		auto guard = mut.try_lock();
		REQUIRE(guard.has_value());
		REQUIRE_EQ(**guard, 0);
	}

	TEST_CASE("lock_shared compiles") {
		shared_mutex<int> mut{};
		REQUIRE_EQ(*mut.lock_shared(), 0);
	}

	TEST_CASE("try_lock_shared compiles") {
		shared_mutex<int> mut{};
		auto guard = mut.try_lock_shared();
		REQUIRE(guard.has_value());
		REQUIRE_EQ(**guard, 0);
	}
}
