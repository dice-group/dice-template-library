#ifndef DICE_TEMPLATELIBRARY_POOLALLOCATOR_HPP
#define DICE_TEMPLATELIBRARY_POOLALLOCATOR_HPP

#include <boost/pool/pool.hpp>

#include <array>
#include <memory>

namespace dice::template_library {

	template<size_t ...bucket_sizes>
	struct pool_allocator_state {
	private:
		std::array<boost::pool<>, sizeof...(bucket_sizes)> pools;

		template<size_t ix, size_t bucket_size, size_t ...rest>
		void *allocate_impl(size_t n_bytes) {
			if (n_bytes < bucket_size) {
				void *ptr = pools[ix].malloc();
				if (ptr == nullptr) [[unlikely]] {
					throw std::bad_alloc{};
				}
				return ptr;
			}

			if constexpr (sizeof...(rest) > 0) {
				return allocate_impl<ix + 1, rest...>(n_bytes);
			} else {
				return new char[n_bytes];
			}
		}

		template<size_t ix, size_t bucket_size, size_t ...rest>
		void deallocate_impl(void *data, size_t n_bytes) {
			if (n_bytes < bucket_size) {
				pools[ix].free(data);
			}

			if constexpr (sizeof...(rest) > 0) {
				return deallocate_impl<ix + 1, rest...>(data, n_bytes);
			} else {
				return delete[] static_cast<char *>(data);
			}
		}

	public:
		void *allocate(size_t n_bytes) {
			return allocate_impl<0, bucket_sizes...>(n_bytes);
		}

		void deallocate(void *data, size_t n_bytes) {
			return deallocate_impl<0, bucket_sizes...>(data, n_bytes);
		}
	};

    template<typename T, size_t ...bucket_sizes>
	struct pool_allocator {
		using value_type = T;
		using pointer = T *;
		using const_pointer = T const *;
		using void_pointer = void *;
		using const_void_pointer = void const *;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;

		/*using propagate_on_container_copy_assignment = typename std::allocator_traits<upstream_allocator_type>::propagate_on_container_copy_assignment;
		using propagate_on_container_move_assignment = typename std::allocator_traits<upstream_allocator_type>::propagate_on_container_move_assignment;
		using propagate_on_container_swap = typename std::allocator_traits<upstream_allocator_type>::propagate_on_container_swap;*/
		using is_always_equal = std::false_type;

    	template<typename U>
    	struct rebind {
    		using other = pool_allocator<U, bucket_sizes...>;
    	};

	private:
		template<typename, size_t ...>
		friend struct pool_allocator;

    	std::shared_ptr<pool_allocator_state<bucket_sizes...>> state_;


	public:
		constexpr pool_allocator()
			: state_{std::make_shared<pool_allocator_state<bucket_sizes...>>()} {
		}

		constexpr pool_allocator(pool_allocator const &other) noexcept = default;
		constexpr pool_allocator(pool_allocator &&other) noexcept = default;
		constexpr pool_allocator &operator=(pool_allocator const &other) noexcept = default;
		constexpr pool_allocator &operator=(pool_allocator &&other) noexcept = default;

		template<typename U>
		constexpr pool_allocator(pool_allocator<U, bucket_sizes...> const &other) noexcept
			: state_{other.state_} {
		}

		constexpr pointer allocate(size_t n) {
			return state_->allocate(sizeof(T) * n);
		}

		constexpr void deallocate(pointer ptr, size_t n) {
			state_->deallocate(ptr, sizeof(T) * n);
		}

		constexpr pool_allocator select_on_container_copy_construction() const {
			return pool_allocator{state_};
		}

		friend constexpr void swap(pool_allocator &a, pool_allocator &b) noexcept {
			using std::swap;
			swap(a.state_, b.state_);
		}

		bool operator==(pool_allocator const &other) const noexcept = default;
		bool operator!=(pool_allocator const &other) const noexcept = default;
	};

} // namespace dice::template_library


#endif // DICE_TEMPLATELIBRARY_POOLALLOCATOR_HPP
