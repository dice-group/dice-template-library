#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/channel.hpp>

#include <string>
#include <thread>
#include <iterator>
#include <ranges>

TEST_SUITE("mpmc_channel") {
	using namespace dice::template_library;

	TEST_CASE("is range") {
		static_assert(std::input_iterator<channel<int>::iterator>);
		static_assert(std::ranges::range<channel<int>>);
		static_assert(std::ranges::input_range<channel<int>>);
	}

	TEST_CASE("sanity check") {
		channel<std::string> chan{3};
		CHECK_FALSE(chan.closed());
		CHECK_EQ(chan.try_pop(), std::nullopt);

		std::string const s{"a"};
		chan.push(s);

		chan.push(std::string{"b"});
		chan.emplace("c");

		// no capacity left
		CHECK_FALSE(chan.try_push(s));
		CHECK_FALSE(chan.try_push(std::string{"b"}));
		CHECK_FALSE(chan.try_emplace("c"));

		chan.close();
		CHECK(chan.closed());

		CHECK_EQ(chan.pop(), "a");
		CHECK_EQ(chan.pop(), "b");
		CHECK_EQ(chan.pop(), "c");
		CHECK_EQ(chan.pop(), std::nullopt);
		CHECK_EQ(chan.pop(), std::nullopt);
		CHECK_EQ(chan.try_pop(), std::nullopt);
		CHECK_EQ(chan.try_pop(), std::nullopt);
	}

	TEST_CASE("usecase sanity check") {
		channel<int> chan{3};

		std::jthread thrd{[&chan]() {
			std::vector<int> ints;
			for (int x : chan) {
				ints.push_back(x);
			}

			CHECK_EQ(ints, std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
		}};

		for (int x = 0; x < 10; ++x) {
			chan.push(x);
		}
		chan.close(); // don't forget to close
	}

	TEST_CASE("iter") {
		channel<std::string> chan{8};
		chan.emplace("a");
		chan.emplace("b");
		chan.close();

		std::vector<std::string> actual;
		for (auto x : chan) {
			actual.push_back(x);
		}

		CHECK_EQ(actual, std::vector<std::string>{"a", "b"});
	}

	TEST_CASE("closed push") {
		channel<std::string> chan{8};
		chan.close();
		CHECK(chan.closed());

		std::string const s{"a"};
		CHECK_FALSE(chan.push(s));
		CHECK_FALSE(chan.try_push(s));

		CHECK_FALSE(chan.push(std::string{"a"}));
		CHECK_FALSE(chan.try_push(std::string{"a"}));
		CHECK_FALSE(chan.emplace("a"));

		CHECK_EQ(chan.pop(), std::nullopt);
		CHECK_EQ(chan.try_pop(), std::nullopt);
	}
}
