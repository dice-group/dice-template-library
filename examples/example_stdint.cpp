#include <dice/template-library/stdint.hpp>

#include <cstdint>
#include <type_traits>

int main() {
	using namespace dice::template_library::literals;

	auto value = 123_u64;
	static_assert(std::is_same_v<decltype(value), uint64_t>);

	return static_cast<int>(value);
}
