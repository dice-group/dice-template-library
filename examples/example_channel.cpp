#include <dice/template-library/channel.hpp>

#include <algorithm>
#include <iostream>
#include <thread>
#include <vector>


int main() {
	dice::template_library::channel<int> chan{8};

	std::jthread thrd{[&chan]() {
		std::vector<int> ints;
		for (int x : chan) {
			ints.push_back(x);
			std::cout << x << ' ';
		}

		assert(std::ranges::equal(ints, std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
	}};

	for (int x = 0; x < 10; ++x) {
		chan.push(x);
	}
	chan.close(); // don't forget to close
}
