#include <dice/template-library/shared_mutex.hpp>

#include <cassert>
#include <thread>

int main() {
	dice::template_library::shared_mutex<int> mut{0};

	std::thread thrd1{[&]() {
		*mut.lock() = 5;
	}};

	thrd1.join();

	std::thread thrd2{[&]() {
		assert(*mut.lock_shared() == 5);
	}};

	thrd2.join();

	assert(*mut.lock() == 5);
}
