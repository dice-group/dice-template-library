#ifndef DICE_TEMPLATELIBRARY_ZST_HPP
#define DICE_TEMPLATELIBRARY_ZST_HPP

#include <cstdint>
#include <type_traits>

namespace dice::template_library {
	namespace detail_zst {
		template<typename T>
		struct zst_checker {
			uint64_t pad;
			[[no_unique_address]] T value;

			static constexpr bool check() noexcept {
				return sizeof(zst_checker) == sizeof(uint64_t);
			}
		};
	} // namespace detail_zst

	/**
	 * Determine if the provided type is a ZST (zero-sized type).
	 * I.e. a type that can benefit from EBO (empty-base-optimization) or [[no_unique_address]].
	 * Note: this is not the same as comparing sizeof(T) to 0, types never have a size of 0
 	 */
	template<typename T>
	struct is_zst : std::bool_constant<detail_zst::zst_checker<T>::check()> {
	};

	template<>
	struct is_zst<void> : std::false_type {};

	template<typename T>
	inline constexpr bool is_zst_v = is_zst<T>::value;

	/**
	 * If From is const make To const as well
	 */
	template<typename From, typename To>
	struct copy_const {
		using type = To;
	};

	template<typename From, typename To>
	struct copy_const<From const, To> {
		using type = To const;
	};

	template<typename From, typename To>
	using copy_const_t = typename copy_const<From, To>::type;

	/**
	 * If From is volatile make To volatile as well
	 */
	template<typename From, typename To>
	struct copy_volatile {
		using type = To;
	};

	template<typename From, typename To>
	struct copy_volatile<From volatile, To> {
		using type = To volatile;
	};

	template<typename From, typename To>
	using copy_volatile_t = typename copy_volatile<From, To>::type;

	/**
	 * If From is const and/or volatile make To const and/or volatile as well
	 */
	template<typename From, typename To>
	struct copy_cv {
		using type = To;
	};

	template<typename From, typename To>
	struct copy_cv<From const, To> {
		using type = To const;
	};

	template<typename From, typename To>
	struct copy_cv<From volatile, To> {
		using type = To volatile;
	};

	template<typename From, typename To>
	struct copy_cv<From const volatile, To> {
		using type = To const volatile;
	};

	/**
	 * Copy the reference qualifiers from From to To
	 */
	template<typename From, typename To>
	using copy_cv_t = typename copy_cv<From, To>::type;

	template<typename From, typename To>
	struct copy_reference {
		using type = To;
	};

	template<typename From, typename To>
	struct copy_reference<From &, To> {
		using type = To &;
	};

	template<typename From, typename To>
	struct copy_reference<From &&, To> {
		using type = To &&;
	};

	template<typename From, typename To>
	using copy_reference_t = typename copy_reference<From, To>::type;


	template<typename From, typename To>
	struct copy_cvref {
		using type = copy_reference_t<From, copy_cv_t<std::remove_reference_t<From>, To>>;
	};

	template<typename From, typename To>
	using copy_cvref_t = typename copy_cvref<From, To>::type;

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_ZST_HPP
