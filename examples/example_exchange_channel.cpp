#include <dice/template-library/exchange_channel.hpp>

#include <cassert>
#include <cstddef>
#include <iostream>
#include <thread>


int main() {
    dice::template_library::exchange_channel<int> chan;

    // only the most recently pushed value is kept
    chan.push(1);
    chan.push(2);
    chan.push(3);
    assert(chan.pop() == 3);
    assert(!chan.try_pop().has_value());

    std::jthread producer{[&chan]() {
        for (int x = 0; x < 1000; ++x) {
            chan.push(x);
        }
        chan.close();  // don't forget to close
    }};

    // consumer only sees the latest value at each read, never an older one
    int last = -1;
    std::size_t observed = 0;
    for (int x : chan) {
        assert(x >= last);
        last = x;
        ++observed;
    }

    std::cout << "consumer observed " << observed << " of 1000 pushed values, last = " << last << '\n';
}