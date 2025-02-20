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
		{ // normal init
			mutex<int> mut{};
			CHECK_EQ(*mut.lock(), 0);
		}

		{ // constexpr init
			static constinit mutex<int> cmut{};
			CHECK_EQ(*cmut.lock(), 0);
		}
	}

	TEST_CASE("copy value inside") {
		std::vector<int> vec{1, 2, 3};
		mutex<std::vector<int>> mut{vec};
		CHECK_EQ(mut.lock()->size(), 3);
	}

	TEST_CASE("move value inside") {
		std::vector<int> vec{1, 2, 3};
		mutex<std::vector<int>> mut{std::move(vec)};
		CHECK_EQ(mut.lock()->size(), 3);
	}

	TEST_CASE("in place construction") {
		struct data {
			int x;
			double y;
		};

		mutex<data> mut{std::in_place, 5, 12.2};
		CHECK_EQ(mut.lock()->x, 5);
		CHECK_EQ(mut.lock()->y, 12.2);
	}

	TEST_CASE("lock compiles") {
		mutex<int> mut{};
		CHECK_EQ(*mut.lock(), 0);
	}

	TEST_CASE("try lock compiles") {
		mutex<int> mut{};
		auto guard = mut.try_lock();
		CHECK(guard.has_value());
		CHECK_EQ(**guard, 0);
	}
}
