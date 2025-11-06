#ifndef DICE_TEMPLATELIBRARY_INPLACEPOLYMORPHIC_HPP
#define DICE_TEMPLATELIBRARY_INPLACEPOLYMORPHIC_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include <variant>

#define DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(noexcept_spec, action_block) \
	if constexpr (noexcept_spec) {                                            \
		action_block                                                          \
		valueless_ = false;                                                   \
	} else {                                                                  \
		try {                                                                 \
			action_block                                                      \
			valueless_ = false;                                               \
		} catch (...) {                                                       \
			valueless_ = true;												  \
			throw;                                                            \
		}                                                                     \
	}

namespace dice::template_library {

	namespace detail_inplace_polymorphic {
		template<typename ...Ts>
		struct first;

		template<typename T, typename ...Ts>
		struct first<T, Ts...> {
			using type = T;
		};

		template<typename B>
		concept has_copy_func = requires (B const &src, B *dst) {
			src.copy_to(dst);
		};

		template<typename B>
		concept has_move_func = requires (B &src, B *dst) {
			src.move_to(dst);
		};
	} // namespace detail_inplace_polymorphic

	/**
	 * This class provides a convenient way to use C++ runtime polymorphism with virtual functions on the stack
	 * instead of the heap.
	 *
	 * This is an alternative to `std::variant` meant for similarly shaped types. It does not require inconvenient visit
	 * or get<> calls. Instead, it uses virtual functions to dispatch to the currently active inner value.
	 *
	 * @tparam Base base class for all Ts...
	 * @tparam Ts derived classes of Base
	 */
	template<typename Base, std::derived_from<Base> ...Ts>
	struct inplace_polymorphic {
		static_assert(sizeof...(Ts) > 0,
					  "A inplace_polymorphic with no Ts... has no valid values.");

		static_assert(std::has_virtual_destructor_v<Base> || (std::is_trivially_destructible_v<Ts> && ...),
					  "Either all polymorphic instances must be able to be safely destroyed via virtual destructor, or they must all be trivially destructable.");

		static_assert(std::is_nothrow_destructible_v<Base> && (std::is_nothrow_destructible_v<Ts> && ...),
					  "Unlike std::variant all destructors are required to be noexcept, instead of just relying on the promise that they do not throw.");

	private:
		using first_derived_type = typename detail_inplace_polymorphic::first<Ts...>::type;
		static constexpr bool noexcept_copy = noexcept(std::declval<inplace_polymorphic>().get_unchecked()->copy_to(std::declval<inplace_polymorphic>().get_unchecked()));
		static constexpr bool noexcept_move = noexcept(std::declval<inplace_polymorphic>().get_unchecked()->move_to(std::declval<inplace_polymorphic>().get_unchecked()));

		alignas(std::max({alignof(Ts)...})) std::byte storage_[std::max({sizeof(Ts)...})];
		bool valueless_ = true;

		void drop() noexcept {
			if constexpr ((std::is_trivially_destructible_v<Ts> && ...)) {
				// noop
			} else {
				if (!valueless_) {
					(**this).~Base();
					valueless_ = true;
				}
			}
		}

		[[nodiscard]] Base *get_unchecked() noexcept {
			return std::launder(reinterpret_cast<Base *>(&storage_));
		}

		[[nodiscard]] Base const *get_unchecked() const noexcept {
			return std::launder(reinterpret_cast<Base const *>(&storage_));
		}

		void copy_from(inplace_polymorphic const &other) noexcept(noexcept_copy) requires (detail_inplace_polymorphic::has_copy_func<Base>) {
			assert(valueless_);

			if (other.valueless_) {
				// no value to copy and this->valueluess_ already set
			} else {
				DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(noexcept_copy, {
					other.get_unchecked()->copy_to(get_unchecked());
				});
			}
		}

		void move_from(inplace_polymorphic &&other) noexcept(noexcept_move) requires (detail_inplace_polymorphic::has_move_func<Base>) {
			assert(valueless_);

			if (other.valueless_) {
				// no value to move and this->valueluess_ already set
			} else {
				DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(noexcept_move, {
					other.get_unchecked()->move_to(get_unchecked());
				});
			}
		}

	public:
		/**
		 * Default construct the first type of Ts...
		 * This function is only enabled if the first type of Ts... is default constructible.
		 */
		inplace_polymorphic() noexcept(std::is_nothrow_default_constructible_v<first_derived_type>) requires (std::is_default_constructible_v<first_derived_type>) {
			DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(std::is_nothrow_default_constructible_v<first_derived_type>, {
				new (&storage_) first_derived_type{};
			});
		}

		/**
		 * Construct the given type with the given arguments.
		 *
		 * @tparam T type to construct
		 * @param args constructor arguments for T
		 */
		template<typename T, typename ...Args>
		explicit inplace_polymorphic(std::in_place_type_t<T>, Args &&...args) noexcept(std::is_nothrow_constructible_v<T, decltype(std::forward<Args>(args))...>) {
			static_assert((std::is_same_v<T, Ts> || ...), "Given type is not in list of possible types");

			DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY((std::is_nothrow_constructible_v<T, decltype(std::forward<Args>(args))...>), {
				new (&storage_) T{std::forward<Args>(args)...};
			});
		}

		/**
		 * Copy the inner value of another inplace_polymorphic into *this.
		 * This function is only enabled if the inner types are virtually-copyable, specifically
		 * if Base has a `virtual void copy_to(Base *dst)` function.
		 *
		 * @param other other instance to copy the inner value from
		 */
		inplace_polymorphic(inplace_polymorphic const &other) noexcept(noexcept_copy) requires (detail_inplace_polymorphic::has_copy_func<Base>) {
			copy_from(other);
		}

		/**
		 * Move the inner value of another inplace_polymorphic into *this.
		 * This function is only enabled if the inner types are virtually-movable, specifically
		 * if Base has a `virtual void move_to(Base *dst)` function.
		 *
		 * @param other other instance to move the inner value from
		 */
		inplace_polymorphic(inplace_polymorphic &&other) noexcept(noexcept_move) requires (detail_inplace_polymorphic::has_move_func<Base>) {
			move_from(std::move(other));
		}

		/**
		 * Copy assign the inner value of another inplace_polymorphic to *this.
		 * This function is only enabled if the inner types are virtually-copyable, specifically
		 * if Base has a `virtual void copy_to(Base *dst)` function.
		 *
		 * @param other other instance to copy the inner value from
		 * @return reference to *this
		 *
		 * @note this function will first destroy the inner value of this and then copy-construct the new value over it.
		 * @note In case the copy operation throws, this inplace_polymorphic is left valueless.
		 */
		inplace_polymorphic &operator=(inplace_polymorphic const &other) noexcept(noexcept_copy) requires (detail_inplace_polymorphic::has_copy_func<Base>) {
			if (this == &other) {
				return *this;
			}

			drop();
			copy_from(other);
			return *this;
		}

		/**
		 * Move assign the inner value of another inplace_polymorphic to *this.
		 * This function is only enabled if the inner types are virtually-movable, specifically
		 * if Base has a `virtual void move_to(Base *dst)` function.
		 *
		 * @param other other instance to move the inner value from
		 * @return reference to *this
		 *
		 * @note This function will first destroy the inner value of this and then move-construct the new value over it.
		 * @note In case the move operation throws, this inplace_polymorphic is left valueless.
		 */
		inplace_polymorphic &operator=(inplace_polymorphic &&other) noexcept(noexcept_move) requires (detail_inplace_polymorphic::has_move_func<Base>) {
			assert(this != &other);

			drop();
			move_from(std::move(other));
			return *this;
		}

		~inplace_polymorphic() {
			drop();
		}

		/**
		 * Creates a new value in-place, in an existing inplace_polymorphic.
		 *
		 * @tparam T type to construct
		 * @param args arguments for constructor of T
		 *
		 * @note This function first destroys the existing value and then constructs the new value over it.
		 * @note In case the constructor throws, this inplace_polymorphic is left valueless.
		 */
		template<typename T, typename ...Args>
		void emplace(Args &&...args) noexcept(std::is_nothrow_constructible_v<T, decltype(std::forward<Args>(args))...>) {
			static_assert((std::is_same_v<T, Ts> || ...), "Given type is not in list of possible types");

			drop();

			DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY((std::is_nothrow_constructible_v<T, decltype(std::forward<Args>(args))...>), {
				new (&storage_) T{std::forward<Args>(args)...};
			});
		}

		/**
		 * @return true if this inplace_polymorphic was left valueless by an assignment or emplace (similar to std::variant::valueless_by_exception)
		 */
		[[nodiscard]] bool valueless_by_exception() const noexcept {
			return valueless_;
		}

		/**
		 * @return Base reference to the inner polymorphic value
		 * @throws std::bad_variant_access if *this is valueluess
		 */
		[[nodiscard]] Base &get() {
			if (valueless_) [[unlikely]] {
				throw std::bad_variant_access{};
			}

			return *get_unchecked();
		}

		/**
		 * @return Base reference to the inner polymorphic value
		 * @throws std::bad_variant_access if *this is valueluess
		 */
		[[nodiscard]] Base const &get() const {
			if (valueless_) [[unlikely]] {
				throw std::bad_variant_access{};
			}

			return *get_unchecked();
		}

		/**
		 * @return Base reference to the inner polymorphic value
		 * @throws std::bad_variant_access if *this is valueluess
		 */
		[[nodiscard]] Base &operator*() {
			return get();
		}

		/**
		 * @return Base pointer to the inner polymorphic value
		 * @throws std::bad_variant_access if *this is valueluess
		 */
		[[nodiscard]] Base *operator->() {
			return &get();
		}

		/**
		 * @return Base reference to the inner polymorphic value
		 * @throws std::bad_variant_access if *this is valueluess
		 */
		[[nodiscard]] Base const &operator*() const {
			return get();
		}

		/**
		 * @return Base pointer to the inner polymorphic value
		 * @throws std::bad_variant_access if *this is valueluess
		 */
		[[nodiscard]] Base const *operator->() const {
			return &get();
		}
	};

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_INPLACEPOLYMORPHIC_HPP
