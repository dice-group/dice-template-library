#include <dice/template-library/lazy_conditional.hpp>

#include <type_traits>
#include <vector>

namespace dice::template_library {

	template<typename T>
	struct unsupported_type {
		static_assert(sizeof(T) == 0, "Unsupported type");
		using type = void;
	};

	template<typename T>
	struct get_value_type {
		using type = typename T::value_type;
	};

	template<typename T>
	struct identity {
		using type = T;
	};

	// Extract value_type from containers, or fail for non-class types
	template<typename T>
	using extract_value_type_t = lazy_conditional_t<
			std::is_class_v<T>,
			get_value_type<T>,
			unsupported_type<T>  // This branch is never instantiated when T is a class
	>;

	// This works because the unsupported_type branch is never instantiated
	static_assert(std::is_same_v<extract_value_type_t<std::vector<int>>, int>);
} // namespace dice::template_library

int main() {
	return 0;
}