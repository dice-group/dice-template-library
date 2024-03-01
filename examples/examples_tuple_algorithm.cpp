#include <dice/template-library/tuple_algorithm.hpp>

#include <cassert>
#include <iostream>
#include <limits>
#include <tuple>

void tuple_fold() {
	std::tuple<int, double, float, long> tup{5, 1.2, 1.3F, 1L};

	auto const sum = dice::template_library::tuple_fold(tup, 0L, [](auto acc, auto x) {
		return acc + static_cast<long>(x);
	});

	assert(sum == 8);
}

void tuple_type_fold() {
	using tuple_t = std::tuple<int, long>;

	auto const digits_sum = dice::template_library::tuple_type_fold<tuple_t>(0UL, []<typename T>(auto acc) {
		return acc + std::numeric_limits<T>::digits10;
	});

	assert(digits_sum == 27);
}

void tuple_for_each() {
	std::tuple<int, double, float> tup{1, 1.0, 1.0F};

	dice::template_library::tuple_for_each(tup, [](auto &x) {
		x += 1;
	});

	assert((tup == std::tuple<int, double, float>{2, 2.0, 2.0F}));
}

void tuple_type_for_each() {
	using tuple_t = std::tuple<int, double, float>;

	dice::template_library::tuple_type_for_each<tuple_t>([&]<typename T>() {
		std::cout << typeid(T).name() << '\n';
	});
}

int main() {
	tuple_fold();
	tuple_type_fold();
	tuple_for_each();
	tuple_type_for_each();
}
