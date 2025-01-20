#ifndef DICE_TEMPLATELIBRARY_ZST_HPP
#define DICE_TEMPLATELIBRARY_ZST_HPP

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

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_ZST_HPP
