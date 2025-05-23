#ifndef DICE_TEMPLATELIBRARY_POOLALLOCATOR_HPP
#define DICE_TEMPLATELIBRARY_POOLALLOCATOR_HPP

#include <boost/pool/pool.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>
#include <memory>

namespace dice::template_library {

	/**
	 * A memory pool or arena that is efficient for allocations which are smaller or equal in size
	 * for one of `bucket_sizes...`.
	 *
	 * The implementation consists of one arena per provided bucket size.
	 * An allocation will be placed into the first bucket where it can fit.
	 * Allocations that do not fit into any bucket are fulfilled with calls to `new`.
	 *
	 * @tparam bucket_sizes allocation sizes for individual elements (in bytes) for the underlying arenas.
	 *		Each size provided here is used to configure the element size of a single arena.
	 *		Importantly, it is **not** the arena chunk size, rather it is the size of elements being placed into the arena.
	 *		The chunk size itself as well as the maximum capacity cannot be configured, they are automatically determined by boost::pool.
	 */
	template<size_t ...bucket_sizes>
	struct pool {
		static_assert(sizeof...(bucket_sizes) > 0,
					  "must at least provide one bucket size, otherwise this would never use a pool for allocation");
		static_assert(std::ranges::is_sorted(std::array<size_t, sizeof...(bucket_sizes)>{bucket_sizes...}),
			          "bucket_sizes parameters must be sorted (small to large)");

		using size_type = size_t;
		using difference_type = std::ptrdiff_t;

	private:
		// note: underlying allocator can not be specified via template parameter
		// because that would be of very limited usefulness, as boost::pool requires the allocation/deallocation functions
		// to be `static`
		using pool_type = boost::pool<boost::default_user_allocator_new_delete>;

		std::array<pool_type, sizeof...(bucket_sizes)> pools_;

		template<size_t ix, size_t bucket_size, size_t ...rest>
		void *allocate_impl(size_t n_bytes) {
			if (n_bytes <= bucket_size) {
				// fits into bucket

				void *ptr = pools_[ix].malloc();
				if (ptr == nullptr) [[unlikely]] {
					// boost::pool uses null-return instead of exception
					throw std::bad_alloc{};
				}
				return ptr;
			}

			if constexpr (sizeof...(rest) > 0) {
				return allocate_impl<ix + 1, rest...>(n_bytes);
			} else {
				// does not fit into any bucket, fall back to new[]
				return new char[n_bytes];
			}
		}

		template<size_t ix, size_t bucket_size, size_t ...rest>
		void deallocate_impl(void *data, size_t n_bytes) {
			if (n_bytes <= bucket_size) {
				// fits into bucket
				pools_[ix].free(data);
				return;
			}

			if constexpr (sizeof...(rest) > 0) {
				deallocate_impl<ix + 1, rest...>(data, n_bytes);
			} else {
				// does not fit into any bucket, must have been allocated via new[]
				delete[] static_cast<char *>(data);
			}
		}

	public:
		pool() : pools_{pool_type{bucket_sizes}...} {
		}

		// underlying implementation does not support copying/moving
		pool(pool const &other) = delete;
		pool(pool &&other) = delete;
		pool &operator=(pool const &other) = delete;
		pool &operator=(pool &&other) = delete;

		~pool() noexcept = default;

		/**
		 * Allocate a chunk of at least `n_bytes` bytes.
		 * In case `n_bytes` is smaller or equal to any of bucket_sizes..., will be allocated
		 * in the smallest bucket it fits in, otherwise the allocation will be directly fulfilled via a call to `new`.
		 *
		 * @param n_bytes number of bytes to allocate
		 * @return (non-null) pointer to allocated region
		 * @throws std::bad_alloc on allocation failure
		 */
		void *allocate(size_t n_bytes) {
			return allocate_impl<0, bucket_sizes...>(n_bytes);
		}

		/**
		 * Deallocate a region previously allocated via `pool::allocate`.
		 *
		 * @param data pointer to the previously allocated region. Note: data must have been allocated by `*this`
		 * @param n_bytes size in bytes of the previously allocated region. Note: `n_bytes` must be the same value as was provided for the call to `allocate` that allocated `data`.
		 */
		void deallocate(void *data, size_t n_bytes) {
			return deallocate_impl<0, bucket_sizes...>(data, n_bytes);
		}
	};

	/**
	 * `std`-style allocator that allocates into an underlying pool.
	 * The bucket size used for allocation is `sizeof(T) * n_elems`.
	 *
	 * @tparam T type to be allocated
	 * @tparam bucket_sizes same as for `pool<bucket_sizes...>`
	 */
	template<typename T, size_t ...bucket_sizes>
	struct pool_allocator {
		using value_type = T;
		using pointer = T *;
		using const_pointer = T const *;
		using void_pointer = void *;
		using const_void_pointer = void const *;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;

		using propagate_on_container_copy_assignment = std::true_type;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_swap = std::true_type;
		using is_always_equal = std::false_type;

    	template<typename U>
    	struct rebind {
    		using other = pool_allocator<U, bucket_sizes...>;
    	};

	private:
		template<typename, size_t ...>
		friend struct pool_allocator;

    	std::shared_ptr<pool<bucket_sizes...>> pool_;

	public:
		/**
		 * Creates a pool_allocator with a default constructed pool
		 */
		pool_allocator()
			: pool_{std::make_shared<pool<bucket_sizes...>>()} {
		}

		explicit pool_allocator(std::shared_ptr<pool<bucket_sizes...>> underlying_pool)
			: pool_{std::move(underlying_pool)} {
		}

		pool_allocator(pool_allocator const &other) noexcept = default;
		pool_allocator(pool_allocator &&other) noexcept = default;
		pool_allocator &operator=(pool_allocator const &other) noexcept = default;
		pool_allocator &operator=(pool_allocator &&other) noexcept = default;
		~pool_allocator() noexcept = default;

		template<typename U>
		pool_allocator(pool_allocator<U, bucket_sizes...> const &other) noexcept
			: pool_{other.pool_} {
		}

		[[nodiscard]] std::shared_ptr<pool<bucket_sizes...>> const &underlying_pool() const noexcept {
			return pool_;
		}

		pointer allocate(size_t n) {
			return static_cast<pointer>(pool_->allocate(sizeof(T) * n));
		}

		void deallocate(pointer ptr, size_t n) {
			pool_->deallocate(ptr, sizeof(T) * n);
		}

		pool_allocator select_on_container_copy_construction() const {
			return pool_allocator{pool_};
		}

		friend void swap(pool_allocator &lhs, pool_allocator &rhs) noexcept {
			using std::swap;
			swap(lhs.pool_, rhs.pool_);
		}

		bool operator==(pool_allocator const &other) const noexcept = default;
		bool operator!=(pool_allocator const &other) const noexcept = default;
	};

} // namespace dice::template_library


#endif // DICE_TEMPLATELIBRARY_POOLALLOCATOR_HPP
