#ifndef DICE_TEMPLATE_LIBRARY_POLYMORPHICALLOCATOR_HPP
#define DICE_TEMPLATE_LIBRARY_POLYMORPHICALLOCATOR_HPP

#include <cstddef>
#include <utility>
#include <variant>

#ifdef DICE_TEMPLATE_LIBRARY_WITH_BOOST
#include <boost/interprocess/offset_ptr.hpp>
#endif//DICE_TEMPLATE_LIBRARY_WITH_BOOST

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

	public:
		constexpr polymorphic_allocator() noexcept(std::is_nothrow_default_constructible_v<typename detail_pmr::first_type<Allocators<T>...>::type>) = default;

		template<typename A, typename ...Args>
		constexpr polymorphic_allocator(std::in_place_type_t<A> inp, Args &&...args) noexcept(std::is_nothrow_constructible_v<A, decltype(std::forward<Args>(args))...>)
			: alloc_{inp, std::forward<Args>(args)...} {
		}

		template<size_t ix, typename ...Args>
		constexpr polymorphic_allocator(std::in_place_index_t<ix> inp, Args &&...args) noexcept(std::is_nothrow_constructible_v<std::variant_alternative_t<ix, inner_variant_t>>)
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

#ifdef DICE_TEMPLATE_LIBRARY_WITH_BOOST
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
#endif//DICE_TEMPLATE_LIBRARY_WITH_BOOST

}// namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_POLYMORPHICALLOCATOR_HPP
