#ifndef DICE_TEMPLATELIBRARY_INPLACEPOLYMORPHIC_HPP
#define DICE_TEMPLATELIBRARY_INPLACEPOLYMORPHIC_HPP

#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include <variant>

#define DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(noexcept_spec, action_block) \
	if constexpr (noexcept_spec) {                                            \
		action_block                                                          \
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

	template<typename Base, std::derived_from<Base> ...Ts>
	struct inplace_polymorphic {
		static_assert(sizeof...(Ts) > 0);
		static_assert(std::has_virtual_destructor_v<Base>);
		static_assert(std::is_nothrow_destructible_v<Base> && (std::is_nothrow_destructible_v<Ts> && ...));

	private:
		alignas(std::max({alignof(Ts)...})) std::byte storage_[std::max({sizeof(Ts)...})];
		bool valueless_;

		void drop() noexcept {
			if (!valueless_) {
				(**this).~Base();
				valueless_ = true;
			}
		}

		void copy_assign_from(inplace_polymorphic const &other) {
			drop();

			if (other.valueless_) {
				assert(valueless_); // set by drop()
			} else {
				DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(noexcept(other->copy_to(**this)), {
					other->copy_to(**this);
				});
			}
		}

		void move_assign_from(inplace_polymorphic &&other) {
			drop();

			if (other.valueless_) {
				assert(valueless_); // set by drop()
			} else {
				DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(noexcept(other->move_to(**this)), {
					other->move_to(**this);
				});
			}
		}

	public:
		inplace_polymorphic() requires (std::is_default_constructible_v<typename detail_inplace_polymorphic::first<Ts...>::type>) {
			using first = typename detail_inplace_polymorphic::first<Ts...>::type;

			DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(std::is_nothrow_default_constructible_v<first>, {
				new (&storage_) first{};
			});
		}

		template<typename T, typename ...Args>
		explicit inplace_polymorphic(std::in_place_type_t<T>, Args &&...args) {
			static_assert((std::is_same_v<T, Ts> || ...));

			DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY((std::is_nothrow_constructible_v<T, decltype(std::forward<Args>(args))...>), {
				new (&storage_) T{std::forward<Args>(args)...};
			});
		}

		inplace_polymorphic(inplace_polymorphic const &other) requires (detail_inplace_polymorphic::has_copy_func<Base>) {
			DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(noexcept(other->copy_to(**this)), {
				other->copy_to(**this);
			});
		}

		inplace_polymorphic(inplace_polymorphic &&other) requires (detail_inplace_polymorphic::has_move_func<Base>) {
			DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY(noexcept(other->move_to(**this)), {
				other->move_to(**this);
			});
		}

		inplace_polymorphic &operator=(inplace_polymorphic const &other) requires (detail_inplace_polymorphic::has_copy_func<Base>) {
			assert(this != &other);

			copy_assign_from(other);
			return *this;
		}

		inplace_polymorphic &operator=(inplace_polymorphic &&other) requires (detail_inplace_polymorphic::has_move_func<Base>) {
			assert(this != &other);

			move_assign_from(std::move(other));
			return *this;
		}

		template<typename T, typename ...Args>
		void emplace(Args &&...args) {
			static_assert((std::is_same_v<T, Ts> || ...));

			drop();

			DICE_TEMPLATELIBRARY_DETAIL_INPLACEPOLY_TRY((std::is_nothrow_constructible_v<T, decltype(std::forward<Args>(args))...>), {
				new (&storage_) T{std::forward<Args>(args)...};
			});
		}

		~inplace_polymorphic() {
			drop();
		}

		[[nodiscard]] Base &get() {
			if (valueless_) [[unlikely]] {
				throw std::bad_variant_access{};
			}

			return *std::launder(reinterpret_cast<Base *>(&storage_));
		}

		[[nodiscard]] Base const &get() const {
			if (valueless_) [[unlikely]] {
				throw std::bad_variant_access{};
			}

			return *std::launder(reinterpret_cast<Base const *>(&storage_));
		}

		[[nodiscard]] Base &operator*() noexcept {
			return get();
		}

		[[nodiscard]] Base *operator->() noexcept {
			return &get();
		}

		[[nodiscard]] Base const &operator*() const noexcept {
			return get();
		}

		[[nodiscard]] Base const &operator->() const noexcept {
			return &get();
		}
	};

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_INPLACEPOLYMORPHIC_HPP
