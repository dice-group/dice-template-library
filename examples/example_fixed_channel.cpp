#include <dice/template-library/fixed_channel.hpp>

#include <iostream>
#include <thread>


int main() {
	// memory for 8 elements is allocated here, push and pop do not allocate
	dice::template_library::fixed_channel<int> chan{8};

	std::jthread thrd{[&chan]() {
		for (int x : chan) {
			std::cout << x << ' ';
		}
	}};

	for (int x = 0; x < 10; ++x) {
		chan.push(x); // blocks while the channel is full
	}
	chan.close(); // don't forget to close
}
