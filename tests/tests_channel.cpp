#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/channel.hpp>
#include <dice/template-library/fixed_channel.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static std::atomic<size_t> allocations = 0;

// the static tsan runtime of clang also defines operator new and delete
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TEST_TSAN_COMPILED 1
#endif
#endif

#ifndef TEST_TSAN_COMPILED
static constexpr bool counts_allocations = true;

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
#else
static constexpr bool counts_allocations = false;
#endif

TEST_SUITE("mpmc_channel") {
	using namespace dice::template_library;

	struct growable {
		template<typename T>
		using type = channel<T>;
	};

	struct fixed {
		template<typename T>
		using type = fixed_channel<T>;
	};

	TEST_CASE_TEMPLATE("is range", F, growable, fixed) {
		using chan_t = typename F::template type<int>;
		static_assert(std::input_iterator<typename chan_t::iterator>);
		static_assert(std::ranges::range<chan_t>);
		static_assert(std::ranges::input_range<chan_t>);
	}

	TEST_CASE_TEMPLATE("sanity check", F, growable, fixed) {
		typename F::template type<std::string> chan{3};
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

	TEST_CASE_TEMPLATE("usecase sanity check", F, growable, fixed) {
		typename F::template type<int> chan{3};

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

	TEST_CASE_TEMPLATE("iter", F, growable, fixed) {
		typename F::template type<std::string> chan{8};
		chan.emplace("a");
		chan.emplace("b");
		chan.close();

		std::vector<std::string> actual;
		for (auto x : chan) {
			actual.push_back(x);
		}

		REQUIRE_EQ(actual, std::vector<std::string>{"a", "b"});
	}

	TEST_CASE_TEMPLATE("closed push", F, growable, fixed) {
		typename F::template type<std::string> chan{8};
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

	struct maybe_throws {
		int value;

		explicit maybe_throws(int value) : value{value} {
			if (value < 0) {
				throw std::runtime_error{"negative"};
			}
		}
	};

	TEST_CASE("push into a growable channel may throw") {
		channel<int> chan{1};
		REQUIRE_FALSE(noexcept(chan.push(1)));
		REQUIRE_FALSE(noexcept(chan.try_push(1)));
		REQUIRE_FALSE(noexcept(chan.emplace(1)));
		REQUIRE_FALSE(noexcept(chan.try_emplace(1)));
	}

	TEST_CASE("push into a fixed channel is noexcept if the element constructor is") {
		fixed_channel<int> chan{1};
		REQUIRE(noexcept(chan.push(1)));
		REQUIRE(noexcept(chan.try_push(1)));
		REQUIRE(noexcept(chan.emplace(1)));
		REQUIRE(noexcept(chan.try_emplace(1)));

		fixed_channel<maybe_throws> tchan{1};
		REQUIRE_FALSE(noexcept(tchan.emplace(1)));
		REQUIRE_FALSE(noexcept(tchan.try_emplace(1)));
	}

	TEST_CASE_TEMPLATE("throwing element constructor", F, growable, fixed) {
		typename F::template type<maybe_throws> chan{2};
		chan.emplace(1);
		REQUIRE_THROWS_AS(chan.emplace(-1), std::runtime_error);
		REQUIRE(chan.try_emplace(2));
		REQUIRE_FALSE(chan.try_emplace(3));
		REQUIRE_EQ(chan.pop()->value, 1);
		REQUIRE_EQ(chan.pop()->value, 2);
		REQUIRE_FALSE(chan.try_pop().has_value());
	}

	using elem256 = std::array<char, 256>;

	TEST_CASE("fixed channel allocates in the constructor only" * doctest::skip(!counts_allocations)) {
		auto const before = allocations.load();
		fixed_channel<elem256> chan{128};
		REQUIRE_EQ(allocations.load(), before + 1);

		for (size_t ix = 0; ix < 128; ++ix) {
			chan.push(elem256{});
		}
		for (size_t ix = 0; ix < 1000; ++ix) {
			(void) chan.pop();
			chan.push(elem256{});
		}
		REQUIRE_EQ(allocations.load(), before + 1);
	}

	TEST_CASE("small growable channel allocates once" * doctest::skip(!counts_allocations)) {
		auto const before = allocations.load();
		channel<elem256> chan{128};  // 128 elements fit into 64 KiB
		REQUIRE_EQ(allocations.load(), before);

		for (size_t ix = 0; ix < 128; ++ix) {
			chan.push(elem256{});
		}
		for (size_t ix = 0; ix < 1000; ++ix) {
			(void) chan.pop();
			chan.push(elem256{});
		}
		while (chan.try_pop().has_value()) {
		}
		chan.push(elem256{});
		REQUIRE_EQ(allocations.load(), before + 1);
	}

	struct big {
		int value;
		std::array<char, 4092> pad{};  // 16 elements fit into 64 KiB
	};

	TEST_CASE("growth and free when empty" * doctest::skip(!counts_allocations)) {
		SUBCASE("kept") {
			channel<big> chan{64};
			auto const before = allocations.load();
			for (int x = 0; x < 64; ++x) {
				chan.push(big{x});
			}
			REQUIRE_EQ(allocations.load(), before + 3);  // 16, 32, 64 elements

			for (int x = 0; x < 64; ++x) {
				REQUIRE_EQ(chan.pop()->value, x);
			}
			chan.push(big{0});
			REQUIRE_EQ(allocations.load(), before + 3);  // a buffer of 4 * 16 elements is kept when empty
		}

		SUBCASE("freed") {
			channel<big> chan{100};
			auto const before = allocations.load();
			for (int x = 0; x < 100; ++x) {
				chan.push(big{x});
			}
			REQUIRE_EQ(allocations.load(), before + 4);  // 16, 32, 64, 100 elements

			for (int x = 0; x < 100; ++x) {
				REQUIRE_EQ(chan.pop()->value, x);
			}
			chan.push(big{0});
			REQUIRE_EQ(allocations.load(), before + 5);  // the buffer of 100 elements was freed when empty
		}
	}

	TEST_CASE("growth while wrapped around") {
		channel<big> chan{64};
		int pushed = 0;
		int popped = 0;
		for (; pushed < 16; ++pushed) {
			chan.push(big{pushed});
		}
		for (; popped < 8; ++popped) {
			REQUIRE_EQ(chan.pop()->value, popped);
		}
		for (; pushed < 25; ++pushed) {
			chan.push(big{pushed});  // wraps around, then grows from 16 to 32 elements
		}
		for (; popped < pushed; ++popped) {
			REQUIRE_EQ(chan.pop()->value, popped);
		}
	}

	TEST_CASE_TEMPLATE("wrap around", F, growable, fixed) {
		typename F::template type<int> chan{100};
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

	TEST_CASE_TEMPLATE("move only element", F, growable, fixed) {
		struct elem {
			std::unique_ptr<int> ptr;
			int const value;  // not assignable
		};

		typename F::template type<elem> chan{3};
		chan.emplace(std::make_unique<int>(0), 0);
		chan.emplace(std::make_unique<int>(1), 1);
		REQUIRE_EQ(*chan.pop()->ptr, 0);
		chan.emplace(std::make_unique<int>(2), 2);
		chan.emplace(std::make_unique<int>(3), 3);  // wraps around
		REQUIRE_EQ(*chan.pop()->ptr, 1);
		chan.close();

		int expected = 2;
		for (auto const &e : chan) {
			REQUIRE_EQ(*e.ptr, expected);
			REQUIRE_EQ(e.value, expected++);
		}
		REQUIRE_EQ(expected, 4);
	}

	struct counted {
		inline static int alive = 0;
		std::array<char, 4096> pad{};  // 16 elements fit into 64 KiB

		counted() noexcept {
			++alive;
		}
		counted(counted const &) noexcept {
			++alive;
		}
		~counted() {
			--alive;
		}
	};

	TEST_CASE_TEMPLATE("every element is destroyed once", F, growable, fixed) {
		{
			typename F::template type<counted> chan{100};  // a growable channel frees its buffer of 100 elements when empty
			for (int x = 0; x < 70; ++x) {
				chan.emplace();
			}
			for (int x = 0; x < 35; ++x) {
				REQUIRE(chan.pop().has_value());
			}
			for (int x = 0; x < 65; ++x) {
				chan.emplace();
			}
			REQUIRE_EQ(counted::alive, 100);

			while (chan.try_pop().has_value()) {
			}
			REQUIRE_EQ(counted::alive, 0);

			for (int x = 0; x < 3; ++x) {
				chan.emplace();
			}
			REQUIRE_EQ(counted::alive, 3);
		}
		REQUIRE_EQ(counted::alive, 0);
	}

	TEST_CASE_TEMPLATE("several producers and consumers", F, growable, fixed) {
		constexpr int prefilled = 1024;
		constexpr int producers = 4;
		constexpr int per_producer = 2000;
		typename F::template type<big> chan{prefilled};
		for (int x = 0; x < prefilled; ++x) {
			chan.push(big{x});  // a growable channel grows to its capacity before the threads start
		}

		std::vector<std::vector<int>> received(4);
		{
			std::vector<std::jthread> consumers;
			for (auto &r : received) {
				consumers.emplace_back([&chan, &r]() {
					for (auto const &e : chan) {
						r.push_back(e.value);
					}
				});
			}

			{
				std::vector<std::jthread> producer_threads;
				for (int p = 0; p < producers; ++p) {
					producer_threads.emplace_back([&chan, p]() {
						for (int x = 0; x < per_producer; ++x) {
							chan.push(big{prefilled + p * per_producer + x});
						}
					});
				}
			}
			chan.close();
		}

		std::vector<int> all;
		for (auto const &r : received) {
			all.insert(all.end(), r.begin(), r.end());
		}
		std::ranges::sort(all);
		REQUIRE(std::ranges::equal(all, std::views::iota(0, prefilled + producers * per_producer)));
	}
}
