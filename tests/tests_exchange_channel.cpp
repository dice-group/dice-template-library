#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/exchange_channel.hpp>

#include <thread>
#include <vector>

TEST_SUITE("exchange_channel") {
    TEST_CASE("sanity check") {
        dice::template_library::exchange_channel<int> w;

        w.push(1);
        CHECK_EQ(w.pop(), 1);
        CHECK_FALSE(w.try_pop().has_value());

        w.push(2);
        w.push(3);
        CHECK_EQ(w.pop(), 3);
        CHECK_FALSE(w.try_pop().has_value());

        w.close();
        CHECK_FALSE(w.pop().has_value());
        CHECK_FALSE(w.try_pop().has_value());
    }

    TEST_CASE("push and emplace return false after close") {
        dice::template_library::exchange_channel<int> ch;
        ch.close();
        CHECK_FALSE(ch.push(1));
        CHECK_FALSE(ch.emplace(2));
    }

    TEST_CASE("closed() reflects state") {
        dice::template_library::exchange_channel<int> ch;
        CHECK_FALSE(ch.closed());
        ch.close();
        CHECK(ch.closed());
    }

    TEST_CASE("pop and try_pop drain pending value after close") {
        SUBCASE("try_pop") {
            dice::template_library::exchange_channel<int> ch;
            ch.push(42);
            ch.close();
            CHECK_EQ(ch.try_pop(), 42);
            CHECK_FALSE(ch.try_pop().has_value());
        }
        SUBCASE("pop") {
            dice::template_library::exchange_channel<int> ch;
            ch.push(42);
            ch.close();
            CHECK_EQ(ch.pop(), 42);
            CHECK_FALSE(ch.pop().has_value());
        }
    }

    TEST_CASE("emplace constructs in-place") {
        dice::template_library::exchange_channel<std::pair<int, int>> ch;
        CHECK(ch.emplace(1, 2));
        CHECK_EQ(ch.pop(), std::make_pair(1, 2));
    }

    TEST_CASE("iterator collects all values") {
        dice::template_library::exchange_channel<int> ch;

        std::thread producer{[&ch] {
            for (int i = 0; i < 5; ++i) {
                ch.push(i);
            }
            ch.close();
        }};

        std::vector<int> received;
        for (int v : ch) {
            received.push_back(v);
        }
        producer.join();

        CHECK_FALSE(received.empty());
        // every received value must be in [0, 4]
        for (int v : received) {
            CHECK(v >= 0);
            CHECK(v <= 4);
        }
        // values must be monotonically non-decreasing (coalescing only keeps latest)
        for (std::size_t i = 1; i < received.size(); ++i) {
            CHECK(received[i] >= received[i - 1]);
        }
    }

    TEST_CASE("close unblocks waiting pop") {
        dice::template_library::exchange_channel<int> ch;

        std::optional<int> result{-1};
        std::thread consumer{[&] {
            result = ch.pop();
        }};

        ch.close();
        consumer.join();

        CHECK_FALSE(result.has_value());
    }

    TEST_CASE("concurrent single producer single consumer") {
        dice::template_library::exchange_channel<int> ch;

        std::thread producer{[&ch] {
            for (int i = 0; i < 100; ++i) {
                ch.push(i);
            }
            ch.close();
        }};

        int last = -1;
        while (auto v = ch.pop()) {
            CHECK(*v >= last);
            last = *v;
        }
        producer.join();
    }
}