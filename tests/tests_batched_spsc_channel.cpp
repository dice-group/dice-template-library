#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/batched_spsc_channel.hpp>

#include <ranges>
#include <thread>

TEST_SUITE("batched_spsc_channel") {
    namespace dtl = dice::template_library;

    static_assert(std::ranges::input_range<dtl::batched_spsc_channel<int>>);

    TEST_CASE("fifo across batches") {
        // 1000 is not a multiple of the batch size, thus close() must flush a partial batch
        dtl::batched_spsc_channel<int> chan{7};
        std::jthread producer{[&chan] {
            for (int ix = 0; ix < 1000; ++ix) {
                CHECK(chan.push(ix));
            }
            chan.close();
        }};

        int expected = 0;
        for (int const value : chan) {
            CHECK_EQ(value, expected++);
        }
        CHECK_EQ(expected, 1000);
    }

    TEST_CASE("empty") {
        dtl::batched_spsc_channel<int> chan;
        CHECK_FALSE(chan.closed());
        chan.close();
        CHECK(chan.closed());
        CHECK(chan.begin() == chan.end());
    }

    TEST_CASE("flush makes a partial batch visible") {
        dtl::batched_spsc_channel<int> chan{100};
        chan.push(1);
        chan.push(2);
        CHECK(chan.flush());

        auto it = chan.begin();
        CHECK_EQ(*it, 1);
        ++it;
        CHECK_EQ(*it, 2);

        chan.close();
        ++it;
        CHECK(it == chan.end());
    }

    TEST_CASE("cancel unblocks the producer") {
        dtl::batched_spsc_channel<int> chan{2};
        std::jthread producer{[&chan] {
            // without a consumer, the producer blocks at the second flush until the cancel
            while (chan.push(0)) {
            }
            CHECK_FALSE(chan.flush());
        }};

        chan.cancel();
    }
}
