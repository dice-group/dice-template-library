#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/channel.hpp>

#include <array>
#include <atomic>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

static std::atomic<size_t> allocations = 0;

void *operator new(size_t size) {
	++allocations;
	if (void *ptr = std::malloc(size); ptr != nullptr) {
		return ptr;
	}
	throw std::bad_alloc{};
}

void operator delete(void *ptr) noexcept {
	std::free(ptr);
}

void operator delete(void *ptr, size_t) noexcept {
	std::free(ptr);
}

TEST_SUITE("mpmc_channel") {
	using namespace dice::template_library;

	TEST_CASE("is range") {
		static_assert(std::input_iterator<channel<int>::iterator>);
		static_assert(std::ranges::range<channel<int>>);
		static_assert(std::ranges::input_range<channel<int>>);
	}

	TEST_CASE("sanity check") {
		channel<std::string> chan{3};
		REQUIRE_FALSE(chan.closed());
		REQUIRE_EQ(chan.try_pop(), std::nullopt);

		std::string const s{"a"};
		chan.push(s);

		chan.push(std::string{"b"});
		chan.emplace("c");

		// no capacity left
		REQUIRE_FALSE(chan.try_push(s));
		REQUIRE_FALSE(chan.try_push(std::string{"b"}));
		REQUIRE_FALSE(chan.try_emplace("c"));

		chan.close();
		REQUIRE(chan.closed());

		REQUIRE_EQ(chan.pop(), "a");
		REQUIRE_EQ(chan.pop(), "b");
		REQUIRE_EQ(chan.pop(), "c");
		REQUIRE_EQ(chan.pop(), std::nullopt);
		REQUIRE_EQ(chan.pop(), std::nullopt);
		REQUIRE_EQ(chan.try_pop(), std::nullopt);
		REQUIRE_EQ(chan.try_pop(), std::nullopt);
	}

	TEST_CASE("usecase sanity check") {
		channel<int> chan{3};

		std::jthread const thrd{[&chan]() {
			std::vector<int> ints;
			for (int const x : chan) {
				ints.push_back(x);
			}

			REQUIRE_EQ(ints, std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
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

		REQUIRE_EQ(actual, std::vector<std::string>{"a", "b"});
	}

	TEST_CASE("closed push") {
		channel<std::string> chan{8};
		chan.close();
		REQUIRE(chan.closed());

		std::string const s{"a"};
		REQUIRE_FALSE(chan.push(s));
		REQUIRE_FALSE(chan.try_push(s));

		REQUIRE_FALSE(chan.push(std::string{"a"}));
		REQUIRE_FALSE(chan.try_push(std::string{"a"}));
		REQUIRE_FALSE(chan.emplace("a"));

		REQUIRE_EQ(chan.pop(), std::nullopt);
		REQUIRE_EQ(chan.try_pop(), std::nullopt);
	}

	TEST_CASE("push may throw") {
		channel<int> chan{1};
		REQUIRE_FALSE(noexcept(chan.push(1)));
		REQUIRE_FALSE(noexcept(chan.try_push(1)));
		REQUIRE_FALSE(noexcept(chan.emplace(1)));
		REQUIRE_FALSE(noexcept(chan.try_emplace(1)));
	}

	TEST_CASE("no allocation up front") {
		auto const before = allocations.load();
		channel<int> chan{std::numeric_limits<size_t>::max()};
		REQUIRE_EQ(allocations.load(), before);

		chan.push(1);
		REQUIRE_EQ(chan.pop(), 1);
	}

	TEST_CASE("no allocation after warm up") {
		using elem = std::array<char, 256>;
		channel<elem> chan{128};
		for (size_t ix = 0; ix < 128; ++ix) {
			chan.push(elem{});
		}

		auto const before = allocations.load();
		for (size_t ix = 0; ix < 1000; ++ix) {
			(void) chan.pop();
			chan.push(elem{});
		}
		REQUIRE_EQ(allocations.load(), before);
	}

	TEST_CASE("wrap around and growth") {
		channel<int> chan{100};
		int pushed = 0;
		int popped = 0;
		while (chan.try_push(pushed)) {
			if (++pushed % 2 == 0) {
				REQUIRE_EQ(chan.pop(), popped++);
			}
		}
		REQUIRE_EQ(pushed - popped, 100);

		chan.close();
		for (int const x : chan) {
			REQUIRE_EQ(x, popped++);
		}
		REQUIRE_EQ(popped, pushed);
	}

	TEST_CASE("move only element") {
		struct elem {
			std::unique_ptr<int> ptr;
			int const value; // not assignable
		};

		channel<elem> chan{3};
		chan.emplace(std::make_unique<int>(0), 0);
		chan.emplace(std::make_unique<int>(1), 1);
		REQUIRE_EQ(*chan.pop()->ptr, 0);
		chan.emplace(std::make_unique<int>(2), 2);
		chan.emplace(std::make_unique<int>(3), 3); // grows while wrapped around
		REQUIRE_EQ(*chan.pop()->ptr, 1);
		REQUIRE_EQ(*chan.pop()->ptr, 2);
		REQUIRE_EQ(*chan.pop()->ptr, 3);
	}

	struct counted {
		static inline int alive = 0;

		counted() noexcept { ++alive; }
		counted(counted const &) noexcept { ++alive; }
		~counted() { --alive; }
	};

	TEST_CASE("every element is destroyed once") {
		{
			channel<counted> chan{8};
			for (int x = 0; x < 6; ++x) {
				chan.emplace();
			}
			for (int x = 0; x < 3; ++x) {
				REQUIRE(chan.pop().has_value());
			}
			for (int x = 0; x < 5; ++x) {
				chan.emplace();
			}
			REQUIRE_EQ(counted::alive, 8);
		}
		REQUIRE_EQ(counted::alive, 0);
	}
}
