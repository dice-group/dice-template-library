#ifndef DICE_TEMPLATE_LIBRARY_OFFSETPTRSTLALLOCATOR_HPP
#define DICE_TEMPLATE_LIBRARY_OFFSETPTRSTLALLOCATOR_HPP

#include <boost/interprocess/offset_ptr.hpp>

namespace dice::template_library {

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

} // namespace dice::template_library

#endif//DICE_TEMPLATE_LIBRARY_OFFSETPTRSTLALLOCATOR_HPP
