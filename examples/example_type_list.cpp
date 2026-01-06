#include <dice/template-library/type_list.hpp>

#include <cassert>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace tl = dice::template_library::type_list;

int main() {
	// Basic type lists
	using numbers = tl::type_list<int, double, float>;
	using chars = tl::type_list<char, bool>;
	using empty = tl::type_list<>;

	// size
	static_assert(tl::size_v<numbers> == 3);
	static_assert(tl::size_v<empty> == 0);

	// first / last
	static_assert(std::is_same_v<tl::first_t<numbers>, int>);
	static_assert(std::is_same_v<tl::last_t<numbers>, float>);
	static_assert(std::is_same_v<tl::opt_t<tl::first<empty>>, tl::nullopt>);

	// nth
	static_assert(std::is_same_v<tl::nth_t<numbers, 0>, int>);
	static_assert(std::is_same_v<tl::nth_t<numbers, 1>, double>);
	static_assert(std::is_same_v<tl::nth_t<numbers, 2>, float>);

	// concat - variadic concatenation
	using combined = tl::concat_t<numbers, chars>;
	static_assert(tl::size_v<combined> == 5);

	using triple = tl::concat_t<numbers, chars, tl::type_list<std::string>>;
	static_assert(tl::size_v<triple> == 6);

	static_assert(tl::size_v<tl::concat_t<>> == 0);
	static_assert(tl::size_v<tl::concat_t<numbers>> == 3);

	// apply - Convert type_list to std::variant, std::tuple, etc.
	using num_variant = tl::apply_t<numbers, std::variant>;
	num_variant v = 3.14;
	assert(std::get<double>(v) == 3.14);

	using num_tuple = tl::apply_t<numbers, std::tuple>;
	num_tuple t{42, 2.71, 1.0f};
	assert(std::get<0>(t) == 42);
	assert(std::get<1>(t) > 2.7 && std::get<1>(t) < 2.8);

	// unpack - Extract types from std::tuple, std::variant, etc. (inverse of apply)
	using unpacked_variant = tl::unpack_t<std::variant<int, double, char>>;
	static_assert(std::is_same_v<unpacked_variant, tl::type_list<int, double, char>>);

	// transform
	constexpr auto add_const = []<typename T>(std::type_identity<T>) {
		return std::add_const<T>{};
	};
	using const_numbers = tl::transform_t<numbers, add_const>;
	static_assert(std::is_same_v<tl::first_t<const_numbers>, int const>);
	static_assert(std::is_same_v<tl::nth_t<const_numbers, 1>, double const>);

	// filter
	using mixed = tl::type_list<int, int const, double, double const, char>;
	constexpr auto is_const = []<typename T>(std::type_identity<T>) {
		return std::is_const_v<T>;
	};
	using only_const = tl::filter_t<mixed, is_const>;
	static_assert(tl::size_v<mixed> == 5);
	static_assert(tl::size_v<only_const> == 2);
	static_assert(std::is_same_v<tl::first_t<only_const>, int const>);

	// generate
	using generated = tl::generate_t<5, []<size_t idx>(std::integral_constant<size_t, idx>) {
		if constexpr (idx % 2 == 0) {
			return std::type_identity<int>{};
		} else {
			return std::type_identity<double>{};
		}
	}>;
	static_assert(tl::size_v<generated> == 5);
	static_assert(std::is_same_v<tl::nth_t<generated, 0>, int>);
	static_assert(std::is_same_v<tl::nth_t<generated, 1>, double>);
	static_assert(std::is_same_v<tl::nth_t<generated, 2>, int>);

	// find_if
	constexpr auto is_double = []<typename T>(std::type_identity<T>) {
		return std::is_same_v<T, double>;
	};
	using found = tl::find_if_t<numbers, is_double>;
	static_assert(std::is_same_v<found, double>);

	// position
	static_assert(tl::position_v<numbers, is_double> == 1);

	// contains
	static_assert(tl::contains_v<numbers, int>);
	static_assert(!tl::contains_v<numbers, char>);

	// all_of, any_of, none_of
	constexpr auto is_arithmetic = []<typename T>(std::type_identity<T>) {
		return std::is_arithmetic_v<T>;
	};
	static_assert(tl::all_of_v<numbers, is_arithmetic>);
	static_assert(!tl::any_of_v<numbers, is_const>);
	static_assert(tl::none_of_v<numbers, is_const>);

	// all_same
	using all_ints = tl::type_list<int, int, int>;
	static_assert(tl::all_same_v<all_ints>);
	static_assert(!tl::all_same_v<numbers>);

	// distinct - Remove duplicate types
	using duplicates = tl::type_list<int, double, int, char, double, float, int>;
	using unique_types = tl::unique_t<duplicates>;
	static_assert(tl::size_v<unique_types> == 4);  // int, double, char, float

	// is_set - Check if all types are unique
	static_assert(tl::all_distinct_v<numbers>);  // int, double, float - all unique
	static_assert(!tl::all_distinct_v<duplicates>);  // has duplicates

	// for_each - runtime iteration
	std::vector<size_t> sizes;
	tl::for_each<numbers>(
			[&sizes]<typename T>(std::type_identity<T>) {
				sizes.push_back(sizeof(T));
			});
	assert(sizes.size() == 3);
	assert(sizes[0] == sizeof(int));
	assert(sizes[1] == sizeof(double));
	assert(sizes[2] == sizeof(float));

	// fold - runtime accumulation
	size_t total_size = tl::fold<numbers>(size_t{0}, []<typename T>(size_t acc, std::type_identity<T>) {
		return acc + sizeof(T);
	});
	assert(total_size == sizeof(int) + sizeof(double) + sizeof(float));

	// opt - Safe access when operations might fail
	static_assert(std::is_same_v<tl::opt_t<tl::first<empty>>, tl::nullopt>);
	static_assert(std::is_same_v<tl::opt_t<tl::first<numbers>>, int>);

	// Real-world example: Type-safe message system
	struct StartMessage {
		int session_id;
	};
	struct DataMessage {
		std::string data;
	};
	struct StopMessage {
		bool force;
	};

	using message_types = tl::type_list<StartMessage, DataMessage, StopMessage>;
	using MessageVariant = tl::apply_t<message_types, std::variant>;
	static_assert(tl::size_v<message_types> == 3);

	MessageVariant msg = DataMessage{"Hello, type_list!"};
	bool visited = false;
	std::visit(
			[&visited](auto &&m) {
				using T = std::decay_t<decltype(m)>;
				if constexpr (std::is_same_v<T, DataMessage>) {
					assert(m.data == "Hello, type_list!");
					visited = true;
				}
			},
			msg);
	assert(visited);

	return 0;
}