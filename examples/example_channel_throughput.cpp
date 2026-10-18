#include <dice/template-library/channel.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <thread>

// one producer and one consumer, prints the time per element
template<size_t elem_size>
void run(size_t capacity, size_t n) {
	using elem = std::array<std::byte, elem_size>;
	dice::template_library::channel<elem> chan{capacity};

	auto const start = std::chrono::steady_clock::now();
	std::jthread producer{[&]() {
		for (size_t ix = 0; ix < n; ++ix) {
			chan.push(elem{});
		}
		chan.close();
	}};

	size_t popped = 0;
	for ([[maybe_unused]] auto const &x : chan) {
		++popped;
	}
	std::chrono::duration<double, std::nano> const elapsed = std::chrono::steady_clock::now() - start;
	std::cout << "elem_size=" << elem_size << " capacity=" << capacity << " n=" << popped << " ns_per_elem=" << elapsed.count() / static_cast<double>(n) << '\n';
}

int main(int argc, char **argv) {
	size_t const n = argc > 1 ? std::stoul(argv[1]) : 10'000;
	for (size_t const capacity : {size_t{1}, size_t{128}}) {
		run<8>(capacity, n);
		run<256>(capacity, n);
	}
}
