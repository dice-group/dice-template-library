#ifndef DICE_TEMPLATE_LIBRARY_POLYMORPHICALLOCATOR_HPP
#define DICE_TEMPLATE_LIBRARY_POLYMORPHICALLOCATOR_HPP

#include <memory>
#include <cstddef>
#include <utility>
#include <variant>

#if __has_include(<boost/interprocess/offset_ptr.hpp>)
#include <boost/interprocess/offset_ptr.hpp>
#endif // __has_include(<boost/interprocess/offset_ptr.hpp>)

namespace dice::template_library {
	namespace detail_pmr {
		/**
		 * Calculates the index of T in the given std::variant Variant
		 */
		template<typename T, typename Variant>
		struct variant_index;

		template<typename T>
		struct variant_index<T, std::variant<>>;

		template<typename T, typename... Ts>
		struct variant_index<T, std::variant<T, Ts...>> {
			static constexpr size_t value = 0;
		};

		template<typename T, typename T1, typename... Ts>
		struct variant_index<T, std::variant<T1, Ts...>> {
			static constexpr size_t value = 1 + variant_index<T, std::variant<Ts...>>::value;
		};

		/**
		 * Similar to std::common_type except that types must be exactly equal
		 * and a static_assertion is triggered if this is not the case (for improved error messages)
		 */
		template<typename ...Ts>
		struct same_type;

		template<typename T>
		struct same_type<T> {
			using type = T;
		};

		template<typename T1, typename T2, typename ...Ts>
		struct same_type<T1, T2, Ts...> {
			static_assert(std::is_same_v<T1, T2>, "Equal types are required here");
			using type = typename same_type<T2, Ts...>::type;
		};

		/**
		 * Counts the occurences of T in Ts...
		 */
		template<typename T, typename ...Ts>
		struct count_type;

		template<typename T>
		struct count_type<T> {
			static constexpr size_t value = 0;
		};

		template<typename T, typename T1, typename ...Ts>
		struct count_type<T, T1, Ts...> {
			static constexpr size_t value = static_cast<size_t>(std::is_same_v<T, T1>) + count_type<T, Ts...>::value;
		};

		/**
		 * Selects the first type from a parameter pack
		 */
		template<typename ...Ts>
		struct first_type;

		template<typename T, typename ...Ts>
		struct first_type<T, Ts...> {
			using type = T;
		};
	}// namespace detail_pmr

	/**
	 * Inspired by std::pmr::polymorphic_allocator except that it uses static polymorphism.
	 * This is mainly useful for scenarios where you cannot have dynamic polymorphism but still want
	 * to be able to have multiple allocators, i.e. projects using persistent memory.
	 *
	 * @tparam T the type of object this allocator allocates
	 * @tparam Allocators a list of the different allocator templates
	 */
	template<typename T, template<typename> typename ...Allocators>
	struct polymorphic_allocator {
		static_assert(sizeof...(Allocators) > 0,
					  "Need at least one allocator");
		static_assert(((detail_pmr::count_type<Allocators<T>, Allocators<T>...>::value == 1) && ...),
					  "Allocator types must only occur exactly once in the parameter list");

		using value_type = typename detail_pmr::same_type<typename std::allocator_traits<Allocators<T>>::value_type...>::type;
		using pointer = typename detail_pmr::same_type<typename std::allocator_traits<Allocators<T>>::pointer...>::type;
		using const_pointer = typename detail_pmr::same_type<typename std::allocator_traits<Allocators<T>>::const_pointer...>::type;
		using void_pointer = typename detail_pmr::same_type<typename std::allocator_traits<Allocators<T>>::void_pointer...>::type;
		using const_void_pointer = typename detail_pmr::same_type<typename std::allocator_traits<Allocators<T>>::const_void_pointer...>::type;

		using propagate_on_container_copy_assignment = std::bool_constant<(std::allocator_traits<Allocators<T>>::propagate_on_container_copy_assignment::value || ...)>;
		using propagate_on_container_move_assignment = std::bool_constant<(std::allocator_traits<Allocators<T>>::propagate_on_container_move_assignment::value || ...)>;
		using propagate_on_container_swap = std::bool_constant<(std::allocator_traits<Allocators<T>>::propagate_on_container_swap::value || ...)>;
		using is_always_equal = std::bool_constant<sizeof...(Allocators) == 1 && std::allocator_traits<typename detail_pmr::first_type<Allocators<T>...>::type>::is_always_equal::value>;

		template<typename U>
		struct rebind {
			using other = polymorphic_allocator<U, Allocators...>;
		};

	private:
		template<typename U, template<typename> typename ...UAllocators>
		friend struct polymorphic_allocator;

		using inner_variant_t = std::variant<Allocators<T>...>;
		inner_variant_t alloc_;

		explicit constexpr polymorphic_allocator(inner_variant_t &&inner)
			: alloc_{std::move(inner)} {
		}

	public:
		constexpr polymorphic_allocator() noexcept(std::is_nothrow_default_constructible_v<typename detail_pmr::first_type<Allocators<T>...>::type>) = default;

		template<typename A, typename ...Args>
		constexpr polymorphic_allocator(std::in_place_type_t<A> inp, Args &&...args) noexcept(std::is_nothrow_constructible_v<A, decltype(std::forward<Args>(args))...>)
			: alloc_{inp, std::forward<Args>(args)...} {
		}

		template<size_t ix, typename ...Args>
		constexpr polymorphic_allocator(std::in_place_index_t<ix> inp, Args &&...args) noexcept(std::is_nothrow_constructible_v<std::variant_alternative_t<ix, inner_variant_t>, decltype(std::forward<Args>(args))...>)
			: alloc_{inp, std::forward<Args>(args)...} {
		}

		template<typename Alloc> requires ((std::is_same_v<std::remove_cvref_t<Alloc>, Allocators<T>> || ...))
		constexpr polymorphic_allocator(Alloc &&alloc)
			: alloc_{std::forward<Alloc>(alloc)} {
		}

		template<typename U>
		constexpr polymorphic_allocator(polymorphic_allocator<U, Allocators...> const &other) noexcept((std::is_nothrow_constructible_v<Allocators<T>, Allocators<U> const &> && ...))
			: alloc_{std::visit(
					  []<typename A>(A const &alloc) -> std::variant<Allocators<T>...> {
						  static constexpr size_t ix = detail_pmr::variant_index<A, std::variant<Allocators<U>...>>::value;
						  return std::variant<Allocators<T>...>{std::in_place_index<ix>, alloc};
					  },
					  other.alloc_)} {
		}

		constexpr polymorphic_allocator(polymorphic_allocator const &other) noexcept((std::is_nothrow_copy_constructible_v<Allocators<T>> && ...)) = default;
		constexpr polymorphic_allocator &operator=(polymorphic_allocator const &other) noexcept((std::is_nothrow_copy_assignable_v<Allocators<T>> && ...)) = default;

		constexpr polymorphic_allocator(polymorphic_allocator &&other) noexcept((std::is_nothrow_move_constructible_v<Allocators<T>> && ...)) = default;
		constexpr polymorphic_allocator &operator=(polymorphic_allocator &&other) noexcept((std::is_nothrow_move_assignable_v<Allocators<T>> && ...)) = default;

		[[nodiscard]] constexpr pointer allocate(size_t n) noexcept((noexcept(std::allocator_traits<Allocators<T>>::allocate(std::declval<Allocators<T> &>(), n)) && ...)) {
			return std::visit([n]<typename A>(A &alloc) {
				return std::allocator_traits<A>::allocate(alloc, n);
			}, alloc_);
		}

		constexpr void deallocate(pointer ptr, size_t n) noexcept((noexcept(std::allocator_traits<Allocators<T>>::deallocate(std::declval<Allocators<T> &>(), ptr, n)) && ...)) {
			std::visit([ptr, n]<typename A>(A &alloc) {
				std::allocator_traits<A>::deallocate(alloc, ptr, n);
			}, alloc_);
		}

		constexpr bool operator==(polymorphic_allocator const &other) const noexcept = default;
		constexpr bool operator!=(polymorphic_allocator const &other) const noexcept = default;

		friend constexpr void swap(polymorphic_allocator &lhs, polymorphic_allocator &rhs) noexcept(noexcept(std::swap(lhs.alloc_, rhs.alloc_))) {
			std::swap(lhs.alloc_, rhs.alloc_);
		}

		constexpr polymorphic_allocator select_on_container_copy_construction() const {
			return polymorphic_allocator{std::visit(
					[]<typename A>(A const &alloc) -> std::variant<Allocators<T>...> {
						return std::allocator_traits<A>::select_on_container_copy_construction(alloc);
					},
					alloc_)};
		}

		/**
		 * Checks if *this currently holds an instance of UAllocator
		 */
		template<typename UAllocator>
		[[nodiscard]] constexpr bool holds_allocator() const noexcept {
			return std::holds_alternative<UAllocator>(alloc_);
		}

		/**
		 * Checks if *this currently holds an instance of UAllocator<T>
		 */
		template<template<typename> typename UAllocator>
		[[nodiscard]] constexpr bool holds_allocator() const noexcept {
			return std::holds_alternative<UAllocator<T>>(alloc_);
		}
	};

	template<typename T, template<typename> typename Allocator>
	struct polymorphic_allocator<T, Allocator> {
		using value_type = typename std::allocator_traits<Allocator<T>>::value_type;
		using pointer = typename std::allocator_traits<Allocator<T>>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator<T>>::const_pointer;
		using void_pointer = typename std::allocator_traits<Allocator<T>>::void_pointer;
		using const_void_pointer = typename std::allocator_traits<Allocator<T>>::const_void_pointer;

		using propagate_on_container_copy_assignment = typename std::allocator_traits<Allocator<T>>::propagate_on_container_copy_assignment;
		using propagate_on_container_move_assignment = typename std::allocator_traits<Allocator<T>>::propagate_on_container_move_assignment;
		using propagate_on_container_swap = typename std::allocator_traits<Allocator<T>>::propagate_on_container_swap;
		using is_always_equal = typename std::allocator_traits<Allocator<T>>::is_always_equal;

		template<typename U>
		struct rebind {
			using other = polymorphic_allocator<U, Allocator>;
		};

	private:
		template<typename U, template<typename> typename ...UAllocators>
		friend struct polymorphic_allocator;

		using inner_type = Allocator<T>;
		inner_type alloc_;

	public:
		constexpr polymorphic_allocator() noexcept(std::is_nothrow_default_constructible_v<Allocator<T>>) = default;

		template<typename ...Args>
		constexpr polymorphic_allocator(std::in_place_type_t<Allocator<T>>, Args &&...args) noexcept(std::is_nothrow_constructible_v<Allocator<T>, decltype(std::forward<Args>(args))...>)
			: alloc_{std::forward<Args>(args)...} {
		}

		template<typename ...Args>
		constexpr polymorphic_allocator(std::in_place_index_t<0>, Args &&...args) noexcept(std::is_nothrow_constructible_v<Allocator<T>, decltype(std::forward<Args>(args))...>)
			: alloc_{std::forward<Args>(args)...} {
		}

		constexpr polymorphic_allocator(Allocator<T> const &alloc) noexcept(std::is_nothrow_copy_constructible_v<Allocator<T>>)
			: alloc_{alloc} {
		}

		constexpr polymorphic_allocator(Allocator<T> &&alloc) noexcept(std::is_nothrow_move_constructible_v<Allocator<T>>)
			: alloc_{std::move(alloc)} {
		}

		template<typename U>
		constexpr polymorphic_allocator(polymorphic_allocator<U, Allocator> const &other) noexcept(std::is_nothrow_constructible_v<Allocator<T>, Allocator<U> const &>)
			: alloc_{other} {
		}

		constexpr polymorphic_allocator(polymorphic_allocator const &other) noexcept(std::is_nothrow_copy_constructible_v<Allocator<T>>) = default;
		constexpr polymorphic_allocator &operator=(polymorphic_allocator const &other) noexcept(std::is_nothrow_copy_assignable_v<Allocator<T>>) = default;

		constexpr polymorphic_allocator(polymorphic_allocator &&other) noexcept(std::is_nothrow_move_constructible_v<Allocator<T>>) = default;
		constexpr polymorphic_allocator &operator=(polymorphic_allocator &&other) noexcept(std::is_nothrow_move_assignable_v<Allocator<T>>) = default;

		[[nodiscard]] constexpr pointer allocate(size_t n) noexcept(noexcept(std::allocator_traits<Allocator<T>>::allocate(std::declval<Allocator<T> &>(), n))) {
			return std::allocator_traits<Allocator<T>>::allocate(alloc_, n);
		}

		constexpr void deallocate(pointer ptr, size_t n) noexcept(noexcept(std::allocator_traits<Allocator<T>>::deallocate(std::declval<Allocator<T> &>(), ptr, n))) {
			return std::allocator_traits<Allocator<T>>::deallocate(alloc_, ptr, n);
		}

		constexpr bool operator==(polymorphic_allocator const &other) const noexcept = default;
		constexpr bool operator!=(polymorphic_allocator const &other) const noexcept = default;

		friend constexpr void swap(polymorphic_allocator &lhs, polymorphic_allocator &rhs) noexcept(noexcept(std::swap(lhs.alloc_, rhs.alloc_))) {
			std::swap(lhs.alloc_, rhs.alloc_);
		}

		constexpr polymorphic_allocator select_on_container_copy_construction() const {
			return polymorphic_allocator{std::allocator_traits<Allocator<T>>::select_on_container_copy_construction(alloc_)};
		}

		/**
		 * Checks if *this currently holds an instance of UAllocator
		 */
		template<typename UAllocator>
		[[nodiscard]] constexpr bool holds_allocator() const noexcept {
			return std::is_same_v<UAllocator, Allocator<T>>;
		}

		/**
		 * Checks if *this currently holds an instance of UAllocator<T>
		 */
		template<template<typename> typename UAllocator>
		[[nodiscard]] constexpr bool holds_allocator() const noexcept {
			return std::is_same_v<UAllocator<T>, Allocator<T>>;
		}
	};

	namespace discriminant2_detail {
		enum struct Discriminant : bool {
			First,
			Second,
		};
	} // namespace discriminant2_detail

	template<typename T, template<typename> typename Allocator1, template<typename> typename Allocator2>
	struct polymorphic_allocator<T, Allocator1, Allocator2> {
		static_assert(!std::is_same_v<Allocator1<T>, Allocator2<T>>,
					  "Allocator types must only occur exactly once in the parameter list");

	private:
		using alloc1_traits = std::allocator_traits<Allocator1<T>>;
		using alloc2_traits = std::allocator_traits<Allocator2<T>>;

	public:
		using value_type = typename detail_pmr::same_type<typename alloc1_traits::value_type,
														  typename alloc2_traits::value_type>::type;

		using pointer = typename detail_pmr::same_type<typename alloc1_traits::pointer,
													   typename alloc2_traits::pointer>::type;

		using const_pointer = typename detail_pmr::same_type<typename alloc1_traits::const_pointer,
															 typename alloc2_traits::const_pointer>::type;

		using void_pointer = typename detail_pmr::same_type<typename alloc1_traits::void_pointer,
															typename alloc2_traits::void_pointer>::type;

		using const_void_pointer = typename detail_pmr::same_type<typename alloc1_traits::const_void_pointer,
																  typename alloc2_traits::const_void_pointer>::type;

		using propagate_on_container_copy_assignment = std::bool_constant<(alloc1_traits::propagate_on_container_copy_assignment::value
																			|| alloc2_traits::propagate_on_container_copy_assignment::value)>;

		using propagate_on_container_move_assignment = std::bool_constant<(alloc1_traits::propagate_on_container_move_assignment::value
																			|| alloc2_traits::propagate_on_container_move_assignment::value)>;

		using propagate_on_container_swap = std::bool_constant<(alloc1_traits::propagate_on_container_swap::value
																 || alloc2_traits::propagate_on_container_swap::value)>;

		using is_always_equal = std::false_type;

		template<typename U>
		struct rebind {
			using other = polymorphic_allocator<U, Allocator1, Allocator2>;
		};

	private:
		template<typename U, template<typename> typename ...UAllocators>
		friend struct polymorphic_allocator;

		using discriminant_type = discriminant2_detail::Discriminant;

		union {
			Allocator1<T> alloc1_;
			Allocator2<T> alloc2_;
		};

		discriminant_type discriminant_;

	public:
		constexpr polymorphic_allocator() noexcept(std::is_nothrow_default_constructible_v<Allocator1<T>>) : discriminant_{discriminant_type::First} {
			new (&alloc1_) Allocator1<T>{};
		}

		template<typename ...Args>
		constexpr polymorphic_allocator(std::in_place_type_t<Allocator1<T>>, Args &&...args) noexcept(std::is_nothrow_constructible_v<Allocator1<T>, decltype(std::forward<Args>(args))...>)
			: discriminant_{discriminant_type::First} {
			new (&alloc1_) Allocator1<T>{std::forward<Args>(args)...};
		}

		template<typename ...Args>
		constexpr polymorphic_allocator(std::in_place_type_t<Allocator2<T>>, Args &&...args) noexcept(std::is_nothrow_constructible_v<Allocator2<T>, decltype(std::forward<Args>(args))...>)
			: discriminant_{discriminant_type::Second} {
			new (&alloc2_) Allocator2<T>{std::forward<Args>(args)...};
		}

		template<typename ...Args>
		constexpr polymorphic_allocator(std::in_place_index_t<0>, Args &&...args) noexcept(std::is_nothrow_constructible_v<Allocator1<T>, decltype(std::forward<Args>(args))...>)
			: discriminant_{discriminant_type::First} {
			new (&alloc1_) Allocator1<T>{std::forward<Args>(args)...};
		}

		template<typename ...Args>
		constexpr polymorphic_allocator(std::in_place_index_t<1>, Args &&...args) noexcept(std::is_nothrow_constructible_v<Allocator2<T>, decltype(std::forward<Args>(args))...>)
			: discriminant_{discriminant_type::Second} {
			new (&alloc2_) Allocator2<T>{std::forward<Args>(args)...};
		}

		constexpr polymorphic_allocator(Allocator1<T> const &alloc) noexcept(std::is_nothrow_copy_constructible_v<Allocator1<T>>)
			: discriminant_{discriminant_type::First} {
			new (&alloc1_) Allocator1<T>{alloc};
		}

		constexpr polymorphic_allocator(Allocator1<T> &&alloc) noexcept(std::is_nothrow_move_constructible_v<Allocator1<T>>)
			: discriminant_{discriminant_type::First} {
			new (&alloc1_) Allocator1<T>{std::move(alloc)};
		}

		constexpr polymorphic_allocator(Allocator2<T> const &alloc) noexcept(std::is_nothrow_copy_constructible_v<Allocator2<T>>)
			: discriminant_{discriminant_type::First} {
			new (&alloc2_) Allocator2<T>{alloc};
		}

		constexpr polymorphic_allocator(Allocator2<T> &&alloc) noexcept(std::is_nothrow_move_constructible_v<Allocator2<T>>)
			: discriminant_{discriminant_type::Second} {
			new (&alloc2_) Allocator2<T>{std::move(alloc)};
		}

		template<typename U>
		constexpr polymorphic_allocator(polymorphic_allocator<U, Allocator1, Allocator2> const &other) noexcept(std::is_nothrow_constructible_v<Allocator1<T>, Allocator1<U> const &>
																												&& std::is_nothrow_constructible_v<Allocator2<T>, Allocator2<U> const &>)
			: discriminant_{other.discriminant_} {

			switch (discriminant_) {
				case discriminant_type::First: {
					new (&alloc1_) Allocator1<T>{other.alloc1_};
					break;
				}
				case discriminant_type::Second: {
					new (&alloc2_) Allocator2<T>{other.alloc2_};
					break;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		constexpr polymorphic_allocator(polymorphic_allocator const &other) noexcept(std::is_nothrow_copy_constructible_v<Allocator1<T>> && std::is_nothrow_copy_constructible_v<Allocator2<T>>)
			: discriminant_{other.discriminant_} {

			switch (discriminant_) {
				case discriminant_type::First: {
					new (&alloc1_) Allocator1<T>{other.alloc1_};
					break;
				}
				case discriminant_type::Second: {
					new (&alloc2_) Allocator2<T>{other.alloc2_};
					break;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		constexpr polymorphic_allocator &operator=(polymorphic_allocator const &other) noexcept(std::is_nothrow_copy_assignable_v<Allocator1<T>>
																								 && std::is_nothrow_destructible_v<Allocator1<T>>
																								 && std::is_nothrow_copy_constructible_v<Allocator1<T>>
																								 && std::is_nothrow_copy_assignable_v<Allocator2<T>>
																								 && std::is_nothrow_destructible_v<Allocator2<T>>
																								 && std::is_nothrow_copy_constructible_v<Allocator2<T>>) {
			if (this == &other) [[unlikely]] {
				return *this;
			}

			switch (discriminant_) {
				case discriminant_type::First: {
					switch (other.discriminant_) {
						case discriminant_type::First: {
							alloc1_ = other.alloc1_;
							break;
						}
						case discriminant_type::Second: {
							alloc1_.~Allocator1<T>();
							new (&alloc2_) Allocator2<T>{other.alloc2_};
							break;
						}
						default: {
							assert(false);
							__builtin_unreachable();
						}
					}
					break;
				}
				case discriminant_type::Second: {
					switch (other.discriminant_) {
						case discriminant_type::First: {
							alloc2_.~Allocator2<T>();
							new (&alloc1_) Allocator1<T>{other.alloc1_};
							break;
						}
						case discriminant_type::Second: {
							alloc2_ = other.alloc2_;
							break;
						}
						default: {
							assert(false);
							__builtin_unreachable();
						}
					}
					break;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}

			discriminant_ = other.discriminant_;
			return *this;
		}

		constexpr polymorphic_allocator(polymorphic_allocator &&other) noexcept(std::is_nothrow_move_constructible_v<Allocator1<T>>
																				 && std::is_nothrow_move_constructible_v<Allocator2<T>>)
			: discriminant_{other.discriminant_} {

			switch (discriminant_) {
				case discriminant_type::First: {
					new (&alloc1_) Allocator1<T>{std::move(other.alloc1_)};
					break;
				}
				case discriminant_type::Second: {
					new (&alloc2_) Allocator2<T>{std::move(other.alloc2_)};
					break;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		constexpr polymorphic_allocator &operator=(polymorphic_allocator &&other) noexcept(std::is_nothrow_move_assignable_v<Allocator1<T>>
																							&& std::is_nothrow_destructible_v<Allocator1<T>>
																							&& std::is_nothrow_move_constructible_v<Allocator1<T>>
																							&& std::is_nothrow_move_assignable_v<Allocator2<T>>
																							&& std::is_nothrow_destructible_v<Allocator2<T>>
																							&& std::is_nothrow_move_constructible_v<Allocator2<T>>) {
			assert(this != &other);

			switch (discriminant_) {
				case discriminant_type::First: {
					switch (other.discriminant_) {
						case discriminant_type::First: {
							alloc1_ = std::move(other.alloc1_);
							break;
						}
						case discriminant_type::Second: {
							alloc1_.~Allocator1<T>();
							new (&alloc2_) Allocator2<T>{std::move(other.alloc2_)};
							break;
						}
						default: {
							assert(false);
							__builtin_unreachable();
						}
					}
					break;
				}
				case discriminant_type::Second: {
					switch (other.discriminant_) {
						case discriminant_type::First: {
							alloc2_.~Allocator2<T>();
							new (&alloc1_) Allocator1<T>{std::move(other.alloc1_)};
							break;
						}
						case discriminant_type::Second: {
							alloc2_ = std::move(other.alloc2_);
							break;
						}
						default: {
							assert(false);
							__builtin_unreachable();
						}
					}
					break;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}

			discriminant_ = other.discriminant_;
			return *this;
		}

		constexpr ~polymorphic_allocator() noexcept(std::is_nothrow_destructible_v<Allocator1<T>> && std::is_nothrow_destructible_v<Allocator2<T>>) {
			switch (discriminant_) {
				case discriminant_type::First: {
					alloc1_.~Allocator1<T>();
					break;
				}
				case discriminant_type::Second: {
					alloc2_.~Allocator2<T>();
					break;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		[[nodiscard]] constexpr pointer allocate(size_t n) noexcept(noexcept(std::allocator_traits<Allocator1<T>>::allocate(std::declval<Allocator1<T> &>(), n))
																	 && noexcept(std::allocator_traits<Allocator2<T>>::allocate(std::declval<Allocator2<T> &>(), n))) {
			switch (discriminant_) {
				case discriminant_type::First: {
					return std::allocator_traits<Allocator1<T>>::allocate(alloc1_, n);
				}
				case discriminant_type::Second: {
					return std::allocator_traits<Allocator2<T>>::allocate(alloc2_, n);
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		constexpr void deallocate(pointer ptr, size_t n) noexcept(noexcept(std::allocator_traits<Allocator1<T>>::deallocate(std::declval<Allocator1<T> &>(), ptr, n))
																   && noexcept(std::allocator_traits<Allocator2<T>>::deallocate(std::declval<Allocator2<T> &>(), ptr, n))) {
			switch (discriminant_) {
				case discriminant_type::First: {
					return std::allocator_traits<Allocator1<T>>::deallocate(alloc1_, ptr, n);
				}
				case discriminant_type::Second: {
					return std::allocator_traits<Allocator2<T>>::deallocate(alloc2_, ptr, n);
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		constexpr bool operator==(polymorphic_allocator const &other) const noexcept {
			if (discriminant_ != other.discriminant_) {
				return false;
			}

			switch (discriminant_) {
				case discriminant_type::First: {
					return alloc1_ == other.alloc1_;
				}
				case discriminant_type::Second: {
					return alloc2_ == other.alloc2_;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}
		constexpr bool operator!=(polymorphic_allocator const &other) const noexcept {
			if (discriminant_ != other.discriminant_) {
				return true;
			}

			switch (discriminant_) {
				case discriminant_type::First: {
					return alloc1_ != other.alloc1_;
				}
				case discriminant_type::Second: {
					return alloc2_ != other.alloc2_;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		// TODO: for some reason variant requires less things to be noexcept https://en.cppreference.com/w/cpp/utility/variant/operator%3D
		// TODO: how are they doing it?
		// TODO: same for other operators
		friend constexpr void swap(polymorphic_allocator &lhs, polymorphic_allocator &rhs) noexcept(std::is_nothrow_swappable_v<Allocator1<T>>
																									 && std::is_nothrow_destructible_v<Allocator1<T>>
																									 && std::is_nothrow_move_constructible_v<Allocator1<T>>
																									 && std::is_nothrow_swappable_v<Allocator2<T>>
																									 && std::is_nothrow_destructible_v<Allocator2<T>>
																									 && std::is_nothrow_move_constructible_v<Allocator2<T>>) {

			switch (lhs.discriminant_) {
				case discriminant_type::First: {
					switch (rhs.discriminant_) {
						case discriminant_type::First: {
							std::swap(lhs.alloc1_, rhs.alloc1_);
							break;
						}
						case discriminant_type::Second: {
							Allocator1<T> tmp{std::move(lhs.alloc1_)};

							lhs.alloc1_.~Allocator1<T>();
							new (&lhs.alloc2_) Allocator2<T>{std::move(rhs.alloc2_)};
							rhs.alloc2_.~Allocator2<T>();
							new (&rhs.alloc1_) Allocator1<T>{std::move(tmp)};
							break;
						}
						default: {
							assert(false);
							__builtin_unreachable();
						}
					}
					break;
				}
				case discriminant_type::Second: {
					switch (rhs.discriminant_) {
						case discriminant_type::First: {
							Allocator2<T> tmp{std::move(lhs.alloc2_)};

							lhs.alloc2_.~Allocator2<T>();
							new (&lhs.alloc1_) Allocator1<T>{std::move(rhs.alloc1_)};
							rhs.alloc1_.~Allocator1<T>();
							new (&rhs.alloc2_) Allocator2<T>{std::move(tmp)};
							break;
						}
						case discriminant_type::Second: {
							std::swap(lhs.alloc2_, rhs.alloc2_);
							break;
						}
						default: {
							assert(false);
							__builtin_unreachable();
						}
					}

					break;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}

			std::swap(lhs.discriminant_, rhs.discriminant_);
		}

		constexpr polymorphic_allocator select_on_container_copy_construction() const {
			switch (discriminant_) {
				case discriminant_type::First: {
					return polymorphic_allocator{std::allocator_traits<Allocator1<T>>::select_on_container_copy_construction(alloc1_)};
				}
				case discriminant_type::Second: {
					return polymorphic_allocator{std::allocator_traits<Allocator2<T>>::select_on_container_copy_construction(alloc2_)};
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		/**
		 * Checks if *this currently holds an instance of UAllocator
		 */
		template<typename UAllocator>
		[[nodiscard]] constexpr bool holds_allocator() const noexcept {
			switch (discriminant_) {
				case discriminant_type::First: {
					return std::is_same_v<UAllocator, Allocator1<T>>;
				}
				case discriminant_type::Second: {
					return std::is_same_v<UAllocator, Allocator2<T>>;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}

		/**
		 * Checks if *this currently holds an instance of UAllocator<T>
		 */
		template<template<typename> typename UAllocator>
		[[nodiscard]] constexpr bool holds_allocator() const noexcept {
			switch (discriminant_) {
				case discriminant_type::First: {
					return std::is_same_v<UAllocator<T>, Allocator1<T>>;
				}
				case discriminant_type::Second: {
					return std::is_same_v<UAllocator<T>, Allocator2<T>>;
				}
				default: {
					assert(false);
					__builtin_unreachable();
				}
			}
		}
	};

#if __has_include(<boost/interprocess/offset_ptr.hpp>)
	/**
	 * @brief Basically std::allocator but returning boost::interprocess::offset_ptr
	 * @tparam T type to allocate
	 */
	template<typename T>
	struct offset_ptr_stl_allocator : std::allocator<T> {
		using value_type = T;
		using pointer = boost::interprocess::offset_ptr<T>;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;

		using propagate_on_container_copy_assignment = typename std::allocator_traits<std::allocator<T>>::propagate_on_container_copy_assignment;
		using propagate_on_container_move_assignment = typename std::allocator_traits<std::allocator<T>>::propagate_on_container_move_assignment;
		using propagate_on_container_swap = typename std::allocator_traits<std::allocator<T>>::propagate_on_container_swap;
		using is_always_equal = typename std::allocator_traits<std::allocator<T>>::is_always_equal;

		constexpr offset_ptr_stl_allocator() noexcept = default;

		template<typename U>
		constexpr offset_ptr_stl_allocator(offset_ptr_stl_allocator<U> const &) noexcept {}

		constexpr pointer allocate(size_t n) {
			return pointer{std::allocator<T>::allocate(n)};
		}

		constexpr void deallocate(pointer ptr, size_t n) {
			return std::allocator<T>::deallocate(std::to_address(ptr), n);
		}
	};
#endif // __has_include(<boost/interprocess/offset_ptr.hpp>)

}// namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_POLYMORPHICALLOCATOR_HPP
