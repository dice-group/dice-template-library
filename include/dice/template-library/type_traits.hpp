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

	template<typename From, typename To>
	using copy_cv_t = typename copy_cv<From, To>::type;


	/**
	 * Copy the reference qualifiers from From to To
	 */
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

	/**
	 * Copy cv and reference qualifiers from From to To
	 */
	template<typename From, typename To>
	struct copy_cvref {
		using type = copy_reference_t<From, copy_cv_t<std::remove_reference_t<From>, To>>;
	};

	template<typename From, typename To>
	using copy_cvref_t = typename copy_cvref<From, To>::type;

	namespace detail_forward_like {
		template<typename T, typename U>
		struct like; // T must be a reference and U an lvalue reference

		template<typename T, typename U>
		struct like<T &, U &> {
			using type = U &;
		};

		template<typename T, typename U>
		struct like<T const &, U&> {
			using type = U const &;
		};

		template<typename T, typename U>
		struct like<T &&, U &> {
			using type = U &&;
		};

		template<typename T, typename U>
		struct like<T const &&, U &> {
			using type = U const &&;
		};

		template<typename T, typename U>
		using like_t = typename like<T &&, U &>::type;
	} // namespace detail_forward_like

	/**
	 *  Forward with the cv-qualifiers and value category of another type.
	 *
	 *  @tparam T An lvalue reference or rvalue reference.
	 *  @tparam U A lvalue reference type deduced from the function argument.
	 *  @param val An lvalue.
	 *  @return `val` converted to match the qualifiers of `T`.
	 *
	 *  @note this exists because clang<21 has a bug that prevents std::forward_like from compiling
	 *  @note code adapted from libstdc++-15
	 */
	template<typename T, typename U>
	[[nodiscard]] constexpr detail_forward_like::like_t<T, U> forward_like(U &&val) noexcept {
		return static_cast<detail_forward_like::like_t<T, U>>(val);
	}

/**
 * Move a value if it is either a non-const rvalue-reference or just a value (without reference qualifiers).
 *
 * @param expr expression to potentially move
 *
 * @example
 * @code
 * int value;
 * int &ref;
 * int const &cref;
 * int &&rref;
 * int const &&crref;
 *
 * DICE_MOVE_IF_VALUE(value) // moves
 * DICE_MOVE_IF_VALUE(rref) // moves
 *
 * DICE_MOVE_IF_VALUE(ref) // does not move
 * DICE_MOVE_IF_VALUE(cref) // does not move
 * DICE_MOVE_IF_VALUE(crref) // does not move
 * @endcode
 */
#define DICE_MOVE_IF_VALUE(expr)                                                                                                              \
    [&]<bool _dice_detail_no_move = std::is_lvalue_reference_v<decltype(expr)> || std::is_const_v<std::remove_reference_t<decltype(expr)>>>() \
            -> std::conditional_t<_dice_detail_no_move, decltype(expr), std::add_rvalue_reference_t<std::remove_cvref_t<decltype(expr)>>> {   \
        if constexpr (_dice_detail_no_move) {                                                                                                 \
            return expr;                                                                                                                      \
        } else {                                                                                                                              \
            return std::move(expr);                                                                                                           \
        }                                                                                                                                     \
    }()

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_ZST_HPP
