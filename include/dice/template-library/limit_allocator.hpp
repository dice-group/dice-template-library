#ifndef DICE_TEMPLATELIBRARY_LIMITALLOCATOR_HPP
#define DICE_TEMPLATELIBRARY_LIMITALLOCATOR_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	/**
	 * The synchronization policy of a limit_allocator
	 */
	enum struct limit_allocator_syncness : bool {
		sync, ///< thread-safe (synchronized)
		unsync, ///< not thread-safe (unsynchronized)
	};

	namespace detail_limit_allocator {
		template<limit_allocator_syncness syncness>
		struct limit_allocator_control_block;

		template<>
		struct limit_allocator_control_block<limit_allocator_syncness::sync> {
			std::atomic<size_t> bytes_left = 0;

			void allocate(size_t n_bytes) {
				auto old = bytes_left.load(std::memory_order_relaxed);

				do {
					if (old < n_bytes) [[unlikely]] {
						throw std::bad_alloc{};
					}
				} while (!bytes_left.compare_exchange_weak(old, old - n_bytes, std::memory_order_relaxed, std::memory_order_relaxed));
			}

			void deallocate(size_t n_bytes) noexcept {
				bytes_left.fetch_add(n_bytes, std::memory_order_relaxed);
			}
		};

		template<>
		struct limit_allocator_control_block<limit_allocator_syncness::unsync> {
			size_t bytes_left = 0;

			void allocate(size_t n_bytes) {
				if (bytes_left < n_bytes) [[unlikely]] {
					throw std::bad_alloc{};
				}
				bytes_left -= n_bytes;
			}

			void deallocate(size_t n_bytes) noexcept {
				bytes_left += n_bytes;
			}
		};
	}// namespace detail_limit_allocator

	/**
	 * Allocator wrapper that limits the amount of memory its underlying allocator
	 * is allowed to allocate.
	 *
	 * @tparam T value type of the allocator (the thing that it allocates)
	 * @tparam Allocator the underlying allocator
	 * @tparam syncness determines the synchronization of the limit
	 */
	template<typename T, template<typename> typename Allocator = std::allocator, limit_allocator_syncness syncness = limit_allocator_syncness::sync>
	struct limit_allocator {
		using control_block_type = detail_limit_allocator::limit_allocator_control_block<syncness>;
		using value_type = T;
		using upstream_allocator_type = Allocator<T>;
		using pointer = typename std::allocator_traits<upstream_allocator_type>::pointer;
		using const_pointer = typename std::allocator_traits<upstream_allocator_type>::const_pointer;
		using void_pointer = typename std::allocator_traits<upstream_allocator_type>::void_pointer;
		using const_void_pointer = typename std::allocator_traits<upstream_allocator_type>::const_void_pointer;
		using size_type = typename std::allocator_traits<upstream_allocator_type>::size_type;
		using difference_type = typename std::allocator_traits<upstream_allocator_type>::difference_type;

		using propagate_on_container_copy_assignment = typename std::allocator_traits<upstream_allocator_type>::propagate_on_container_copy_assignment;
		using propagate_on_container_move_assignment = typename std::allocator_traits<upstream_allocator_type>::propagate_on_container_move_assignment;
		using propagate_on_container_swap = typename std::allocator_traits<upstream_allocator_type>::propagate_on_container_swap;
		using is_always_equal = std::false_type;

		template<typename U>
		struct rebind {
			using other = limit_allocator<U, Allocator, syncness>;
		};

	private:
		template<typename, template<typename> typename, limit_allocator_syncness>
		friend struct limit_allocator;

		std::shared_ptr<control_block_type> control_block_;
		[[no_unique_address]] upstream_allocator_type inner_;

		constexpr limit_allocator(std::shared_ptr<control_block_type> const &control_block, upstream_allocator_type const &alloc)
                requires (std::is_default_constructible_v<upstream_allocator_type>)
            : control_block_{control_block},
              inner_{alloc} {
        }

	public:
        explicit constexpr limit_allocator(size_t bytes_limit)
                requires (std::is_default_constructible_v<upstream_allocator_type>)
            : control_block_{std::make_shared<control_block_type>(bytes_limit)},
              inner_{} {
        }

		constexpr limit_allocator(limit_allocator const &other) noexcept(std::is_nothrow_move_constructible_v<upstream_allocator_type>) = default;
		constexpr limit_allocator(limit_allocator &&other) noexcept(std::is_nothrow_copy_constructible_v<upstream_allocator_type>) = default;
		constexpr limit_allocator &operator=(limit_allocator const &other) noexcept(std::is_nothrow_copy_assignable_v<upstream_allocator_type>) = default;
		constexpr limit_allocator &operator=(limit_allocator &&other) noexcept(std::is_nothrow_move_assignable_v<upstream_allocator_type>) = default;
		constexpr ~limit_allocator() = default;

		template<typename U>
		constexpr limit_allocator(limit_allocator<U, Allocator> const &other) noexcept(std::is_nothrow_constructible_v<upstream_allocator_type, typename limit_allocator<U, Allocator>::upstream_allocator_type const &>)
			: control_block_{other.control_block_},
			  inner_{other.inner_} {
		}

		constexpr limit_allocator(size_t bytes_limit, upstream_allocator_type const &upstream)
			: control_block_{std::make_shared<control_block_type>(bytes_limit)},
			  inner_{upstream} {
		}

		constexpr limit_allocator(size_t bytes_limit, upstream_allocator_type &&upstream)
			: control_block_{std::make_shared<control_block_type>(bytes_limit)},
			  inner_{std::move(upstream)} {
		}

		template<typename ...Args>
		explicit constexpr limit_allocator(size_t bytes_limit, std::in_place_t, Args &&...args)
			: control_block_{std::make_shared<control_block_type>(bytes_limit)},
			  inner_{std::forward<Args>(args)...} {
		}

		constexpr pointer allocate(size_t n) {
			control_block_->allocate(n * sizeof(T));

			try {
				return std::allocator_traits<upstream_allocator_type>::allocate(inner_, n);
			} catch (...) {
				control_block_->deallocate(n * sizeof(T));
				throw;
			}
		}

		constexpr void deallocate(pointer ptr, size_t n) {
			std::allocator_traits<upstream_allocator_type>::deallocate(inner_, ptr, n);
			control_block_->deallocate(n * sizeof(T));
		}

		constexpr limit_allocator select_on_container_copy_construction() const {
			return limit_allocator{control_block_, std::allocator_traits<upstream_allocator_type>::select_on_container_copy_construction(inner_)};
		}

		[[nodiscard]] upstream_allocator_type const &upstream_allocator() const noexcept {
			return inner_;
		}

        friend constexpr void swap(limit_allocator &a, limit_allocator &b) noexcept(std::is_nothrow_swappable_v<upstream_allocator_type>)
                requires (std::is_swappable_v<upstream_allocator_type>)
        {
			using std::swap;
			swap(a.control_block_, b.control_block_);
			swap(a.inner_, b.inner_);
		}

		bool operator==(limit_allocator const &other) const noexcept = default;
		bool operator!=(limit_allocator const &other) const noexcept = default;
	};
}// namespace dice::template_library

#endif// DICE_TEMPLATELIBRARY_LIMITALLOCATOR_HPP