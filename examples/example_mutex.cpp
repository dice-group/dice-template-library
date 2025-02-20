#include <dice/template-library/mutex.hpp>

#include <cassert>
#include <thread>

int main() {
	dice::template_library::mutex<int> mut{0};

	std::thread thrd{[&]() {
		*mut.lock() = 5;
	}};

	thrd.join();
	assert(*mut.lock() == 5);
}
