#include <cassert>
#include <thread>

import dice.template_library;

int main() {
	dice::template_library::mutex<int> mut{0};

	std::thread thrd{[&]() {
		*mut.lock() = 5;
	}};

	thrd.join();
	assert(*mut.lock() == 5);
}
