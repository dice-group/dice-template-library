#include <dice/template-library/flex_array.hpp>

#include <cstddef>
#include <iostream>
#include <variant>

using namespace dice::template_library;

// multidimensional-shape polymorphism without heap allocation

static constexpr size_t shape_max_dim = 2;
using shape_extents = flex_array<size_t, dynamic_extent, shape_max_dim>;

struct point {
	flex_array<size_t, 0> extent;

	[[nodiscard]] shape_extents get_extents() const noexcept {
		return extent;
	}
};

struct line {
	flex_array<size_t, 1> length;

	[[nodiscard]] shape_extents get_extents() const noexcept {
		return length;
	}
};

struct square {
	flex_array<size_t, 2> width_height;

	[[nodiscard]] shape_extents get_extents() const noexcept {
		return width_height;
	}
};

struct shape {
	std::variant<point, line, square> shape_;

	[[nodiscard]] shape_extents get_extents() const noexcept {
		return std::visit([](auto const &sha) {
			return sha.get_extents();
		}, shape_);
	}
};

// minimal size of static-length flex_arrays
static_assert(sizeof(point) == 1);
static_assert(sizeof(line) == sizeof(size_t));
static_assert(sizeof(square) == sizeof(size_t) * 2);

// no heap allocations on shape_extents
static_assert(sizeof(shape_extents) == sizeof(size_t) * 2 + sizeof(size_t));

void print_extents(shape const &sha) {
	for (auto const ext : sha.get_extents()) {
		std::cout << ext << " ";
	}
}

int main() {
	shape const point1{point{.extent = {}}};
	shape const line1{line{.length = {12}}};
	shape const square1{square{.width_height = {52, 15}}};

	print_extents(point1);
	print_extents(line1);
	print_extents(square1);
}
