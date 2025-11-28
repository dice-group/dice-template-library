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
			unsupported_type<T>// This branch is never instantiated when T is a class
			>;

	// This works because the unsupported_type branch is never instantiated
	static_assert(std::is_same_v<extract_value_type_t<std::vector<int>>, int>);

	/**
	 * Example: Using lazy_switch for multi-way type selection
	 */

	template<typename T>
	struct integral_result {
		using type = long;
	};

	template<typename T>
	struct floating_result {
		using type = double;
	};

	template<typename T>
	struct pointer_result {
		using type = void *;
	};

	template<typename T>
	struct default_unsupported {
		static_assert(sizeof(T) == 0, "Unsupported type");
		using type = void;
	};

	// Multi-way selection based on type traits
	template<typename T>
	using classify_type = lazy_switch_default_t<
			default_unsupported<T>,
			case_<std::is_integral_v<T>, integral_result<T>>,
			case_<std::is_floating_point_v<T>, floating_result<T>>,
			case_<std::is_pointer_v<T>, pointer_result<T>>>;

	static_assert(std::is_same_v<classify_type<int>, long>);
	static_assert(std::is_same_v<classify_type<float>, double>);
	static_assert(std::is_same_v<classify_type<char *>, void *>);

	// The default_unsupported branch is never instantiated for valid types,
	// so its static_assert doesn't fire

	/**
	 * Example: Using lazy_switch_no_default when you know a case will always match
	 */

	template<typename T>
	using classify_type_no_default = lazy_switch_t<
			case_<std::is_integral_v<T>, integral_result<T>>,
			case_<std::is_floating_point_v<T>, floating_result<T>>,
			case_<std::is_pointer_v<T>, pointer_result<T>>>;

	static_assert(std::is_same_v<classify_type_no_default<int>, long>);
	static_assert(std::is_same_v<classify_type_no_default<double>, double>);
	static_assert(std::is_same_v<classify_type_no_default<void *>, void *>);

	// No default case needed - cleaner when you know one case will always match

}// namespace dice::template_library

int main() {
	return 0;
}