#ifndef DICE_TEMPLATELIBRARY_FLEXARRAY_HPP
#define DICE_TEMPLATELIBRARY_FLEXARRAY_HPP

#include <dice/template-library/functional.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstdlib>
#include <iterator>
#include <span>
#include <stdexcept>
#include <vector>

#if __has_include(<ankerl/svector.h>)
#include <ankerl/svector.h>
#endif // __has_include

namespace dice::template_library {
    using std::dynamic_extent;

	/**
	 * The underlying implementation of a flex array
	 */
	enum struct flex_array_mode {
		direct_static_size, ///< size is static and flex array is stack allocated
		direct_dynamic_limited_size, ///< size is dynamic but limited by max_size, flex array is stack allocated and has at most max_size elements
		sbo_dynamic_size, ///< small buffer optimized vector
		dynamic_size, ///< regular vector
	};

	namespace detail_flex_array {
		/**
		 * The element storage of a direct mode with room for no element (extent 0 or max_extent 0).
		 * It is empty.
		 */
		template<typename T>
		struct no_elements {
			[[nodiscard]] static constexpr T *data() noexcept {
				return nullptr;
			}

			/**
			 * There is no element, so no index is valid.
			 */
			T &operator[](size_t) const noexcept {
			    assert(false);
				__builtin_unreachable();
			}

			constexpr bool operator==(no_elements const &) const noexcept = default;
		    constexpr std::strong_ordering operator<=>(no_elements const &) const noexcept = default;
		};

		template<typename T, size_t n>
		struct c_array {
			using type = T[n];
		};

		template<typename T>
		struct c_array<T, 0> {
			using type = no_elements<T>;
		};

		/**
		 * The element storage of the direct modes: the C array `T[n]`.
		 * A C array of length 0 is not valid C++, so for `n == 0` it is `no_elements<T>`.
		 */
		template<typename T, size_t n>
		using c_array_t = c_array<T, n>::type;

		template<typename T, size_t extent, size_t max_extent>
		struct flex_array_inner;


		template<typename T, size_t extent> requires (extent != dynamic_extent)
		struct flex_array_inner<T, extent, extent> {
			// fully fixed size
			static constexpr flex_array_mode mode = flex_array_mode::direct_static_size;

			static constexpr size_t size_ = extent;
			c_array_t<T, extent> data_;

			static constexpr size_t size() noexcept {
				return size_;
			}

			[[nodiscard]] constexpr T *data() noexcept {
				return std::data(data_);
			}

			[[nodiscard]] constexpr T const *data() const noexcept {
				return std::data(data_);
			}

			operator std::span<T, extent>() noexcept {
				return std::span<T, extent>{data(), size_};
			}

			operator std::span<T const, extent>() const noexcept {
				return std::span<T const, extent>{data(), size_};
			}

			constexpr bool operator==(flex_array_inner const &) const noexcept = default;

			constexpr auto operator<=>(flex_array_inner const &other) const noexcept requires requires (T const &x) { synth_three_way{}(x, x); } {
				return std::lexicographical_compare_three_way(data(), data() + size_, other.data(), other.data() + size_, synth_three_way{});
			}
		};

		template<typename T, size_t max_extent>
		struct flex_array_inner<T, dynamic_extent, max_extent> {
			// fixed max size, dynamic actual size
			static constexpr flex_array_mode mode = flex_array_mode::direct_dynamic_limited_size;

			size_t size_ = 0;
			c_array_t<T, max_extent> data_;

			[[nodiscard]] constexpr size_t size() const noexcept {
				return size_;
			}

			constexpr void set_size(size_t size) noexcept {
				assert(size <= max_extent);
				size_ = size;
			}

			[[nodiscard]] constexpr T *data() noexcept {
				return std::data(data_);
			}

			[[nodiscard]] constexpr T const *data() const noexcept {
				return std::data(data_);
			}

			operator std::span<T>() noexcept {
				return {data(), size_};
			}

			operator std::span<T const>() const noexcept {
				return {data(), size_};
			}

			constexpr bool operator==(flex_array_inner const &other) const noexcept requires requires (T const &x) { x == x; } {
				return std::equal(data(), data() + size(), other.data(), other.data() + other.size());
			}

			constexpr auto operator<=>(flex_array_inner const &other) const noexcept requires requires (T const &x) { synth_three_way{}(x, x); } {
				return std::lexicographical_compare_three_way(data(), data() + size(), other.data(), other.data() + other.size(), synth_three_way{});
			}
		};

		template<typename T>
		struct flex_array_inner<T, dynamic_extent, dynamic_extent> {
			static constexpr flex_array_mode mode = flex_array_mode::dynamic_size;

			std::vector<T> data_;

			[[nodiscard]] size_t size() const noexcept {
				return data_.size();
			}

			[[nodiscard]] constexpr T *data() noexcept {
				return data_.data();
			}

			[[nodiscard]] constexpr T const *data() const noexcept {
				return data_.data();
			}

			operator std::span<T>() noexcept {
				return data_;
			}

			operator std::span<T const>() const noexcept {
				return data_;
			}

			void set_size(size_t size) {
				data_.resize(size);
			}

			bool operator==(flex_array_inner const &other) const noexcept = default;
			auto operator<=>(flex_array_inner const &other) const noexcept = default;
		};

#if __has_include(<ankerl/svector.h>)
		template<typename T, size_t extent> requires (extent != dynamic_extent)
		struct flex_array_inner<T, extent, dynamic_extent> {
			// dynamic max size, fixed small buffer size
			static constexpr flex_array_mode mode = flex_array_mode::sbo_dynamic_size;

			::ankerl::svector<T, extent> data_;

			[[nodiscard]] size_t size() const noexcept {
				return data_.size();
			}

			[[nodiscard]] constexpr T *data() noexcept {
				return data_.data();
			}

			[[nodiscard]] constexpr T const *data() const noexcept {
				return data_.data();
			}

			operator std::span<T>() noexcept {
				return data_;
			}

			operator std::span<T const>() const noexcept {
				return data_;
			}

			void set_size(size_t size) {
				data_.resize(size);
			}

			constexpr bool operator==(flex_array_inner const &other) const noexcept requires requires (T const &x) { x == x; } {
				return std::equal(data(), data() + size(), other.data(), other.data() + other.size());
			}

			constexpr auto operator<=>(flex_array_inner const &other) const noexcept requires requires (T const &x) { synth_three_way{}(x, x); } {
				return std::lexicographical_compare_three_way(data(), data() + size(), other.data(), other.data() + other.size(), synth_three_way{});
			}
		};
#else // __has_include
		template<typename T, size_t extent> requires (extent != dynamic_extent)
		struct flex_array_inner<T, extent, dynamic_extent> {
			template<typename X>
			static constexpr bool always_false() {
				// workaround for static_assert(false) always asserting
				return false;
			}

			static_assert(always_false<flex_array_inner>(), "Could not find <ankerl/svector.h>, flex_array_implementation::sbo_dynamic_size mode is not available.");
		};
#endif // __has_include
	} // namespace detail_flex_array

	/**
	 * A combination of std::array, std::span and a small buffer optimized vector.
	 * The extent_ and max_extent_ template parameters select one of four modes:
	 *
	 * - extent_ == max_extent_ == some integer value: behaves like std::array.
	 *   The size is fixed and statically known, storage is inline.
	 * - extent_ == dynamic_extent, max_extent_ == some integer value: behaves like static_vector.
	 *   The size is a runtime value up to max_extent_, storage is inline.
	 *   Constructors and resize throw when max_extent_ is exceeded.
	 * - extent_ == some integer value, max_extent_ == dynamic_extent: behaves like a
	 *   small buffer optimized vector (ankerl::svector, requires <ankerl/svector.h>).
	 *   Up to extent_ elements are stored inline, larger sizes allocate on the heap.
	 *   There is no size limit: constructors and resize grow instead of throwing.
	 * - extent_ == max_extent_ == dynamic_extent: behaves like std::vector.
	 *
	 * @tparam T value type
	 * @tparam extent_ extent of the flex array
	 * @tparam max_extent_ max extent of the flex array, defaults to extent_
	 */
	template<typename T, size_t extent_, size_t max_extent_ = extent_>
	struct flex_array {
	private:
		using inner_type = detail_flex_array::flex_array_inner<T, extent_, max_extent_>;

	public:
		static constexpr size_t extent = extent_;
		static constexpr size_t max_extent = max_extent_;

		static constexpr bool has_max_extent = max_extent != dynamic_extent;
		static constexpr bool has_dynamic_extent = extent == dynamic_extent || max_extent == dynamic_extent;
		static constexpr flex_array_mode mode = inner_type::mode;

		using value_type = T;
		using reference = value_type &;
		using const_reference = value_type const &;
		using pointer = value_type *;
		using const_pointer = value_type const *;
		using size_type = size_t;
		using iterator = value_type *;
		using const_iterator = value_type const *;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	private:
		inner_type inner_;

	public:
		constexpr flex_array() noexcept = default;
		constexpr flex_array(flex_array const &) noexcept = default;
		constexpr flex_array(flex_array &&) noexcept = default;
		constexpr flex_array &operator=(flex_array const &) noexcept = default;
		constexpr flex_array &operator=(flex_array &&) noexcept = default;
		constexpr ~flex_array() noexcept = default;

		/**
		 * Initializes the flex_array using an initializer_list
		 *
		 * @param init initializer list
		 * @throws std::length_error if extent == dynamic_extend and init.size() exceeds max_size()
		 *			or extent != dynamic_extent and init.size() != extent
		 */
		constexpr flex_array(std::initializer_list<value_type> const init) {
			if constexpr (has_max_extent) {
				if (init.size() > max_size()) [[unlikely]] {
					throw std::length_error{"flex_array::flex_array: maximum size exceeded"};
				}
			}

			if constexpr (has_dynamic_extent) {
				inner_.set_size(init.size());
			} else {
				if (init.size() != extent) [[unlikely]] {
					throw std::length_error{"flex_array::flex_array: size mismatch"};
				}
			}

			std::ranges::copy(init, begin());
		}

		/**
		 * Initializes the flex_array using the range [first, last)
		 *
		 * @param first iterator to first element
		 * @param last sentinel of the range
		 * @throws std::length_error if distance(first, last) exceeds max_size()
		 */
		template<std::input_iterator Iter, std::sentinel_for<Iter> Sent>
		constexpr flex_array(Iter first, Sent last) {
			if constexpr (has_max_extent && std::random_access_iterator<Iter>) {
				auto const range_size = std::distance(first, last);
				if (static_cast<size_t>(range_size) > max_size()) [[unlikely]] {
					throw std::length_error{"flex_array::flex_array: maximum size exceeded"};
				}
			}

			size_t ix = 0;
			while (first != last) {
				if constexpr (has_max_extent) {
					if constexpr (!std::random_access_iterator<Iter>) {
						if (ix >= max_size()) [[unlikely]] {
							throw std::length_error{"flex_array::flex_array: maximum size exceeded"};
						}
					}

					inner_.data_[ix++] = *first++;
				} else {
					inner_.data_.push_back(*first++);
				}
			}

			if constexpr (extent == dynamic_extent && max_extent != dynamic_extent) {
				inner_.set_size(ix);
			}
		}

		/**
		 * Converts from a flex_array of dynamic extent to a flex_array of static extent
		 *
		 * @throws std::length_error if other.size() != extent
		 */
		template<size_t other_extent, size_t other_max_extent>
		explicit constexpr flex_array(flex_array<value_type, other_extent, other_max_extent> const &other)
		requires (std::remove_cvref_t<decltype(other)>::has_dynamic_extent && !has_dynamic_extent) {
			if (other.size() != extent) [[unlikely]] {
				throw std::length_error{"flex_array::flex_array: size mismatch"};
			}

			std::ranges::copy(other, begin());
		}

		/**
		 * Converts from a flex_array of static extent to a flex_array of dynamic extent
		 */
		template<size_t other_extent>
		constexpr flex_array(flex_array<value_type, other_extent> const &other) noexcept requires (has_dynamic_extent && other_extent != dynamic_extent) {
			static_assert(other_extent <= max_extent, "extent of other is too large for this flex_array");

			inner_.set_size(other.size());
			std::ranges::copy(other, begin());
		}

		constexpr operator std::span<value_type, has_dynamic_extent ? dynamic_extent : extent>() noexcept {
			return inner_;
		}

		constexpr operator std::span<value_type const, has_dynamic_extent ? dynamic_extent : extent>() const noexcept {
			return inner_;
		}

		/**
		 * The maximum size. Note: this is dynamic_extent, i.e. not a real limit,
		 * for the growable modes (max_extent_ == dynamic_extent).
		 */
		[[nodiscard]] static constexpr size_type max_size() noexcept { return max_extent; }
		[[nodiscard]] constexpr size_type size() const noexcept { return inner_.size(); }
		[[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

		/**
		 * Sets the size to new_size.
		 * With max_extent_ == dynamic_extent the array grows as needed,
		 * beyond the inline buffer if necessary.
		 *
		 * @throws std::invalid_argument if max_extent_ != dynamic_extent and new_size > max_extent_
		 */
		void resize(size_type new_size) requires (has_dynamic_extent) {
			if constexpr (has_max_extent) {
				if (new_size > max_extent) [[unlikely]] {
					throw std::invalid_argument{"flex_array::resize: new_size exceeds max_extent"};
				}
			}

			inner_.set_size(new_size);
		}

		[[nodiscard]] constexpr pointer data() noexcept { return inner_.data(); }
		[[nodiscard]] constexpr const_pointer data() const noexcept { return inner_.data(); }

		constexpr iterator begin() noexcept { return inner_.data(); }
		constexpr iterator end() noexcept { return std::next(begin(), size()); }
		constexpr const_iterator begin() const noexcept { return inner_.data(); }
		constexpr const_iterator end() const noexcept { return std::next(begin(), size()); }
		constexpr const_iterator cbegin() const noexcept { return inner_.data(); }
		constexpr const_iterator cend() const noexcept { return std::next(cbegin(), size()); }
		constexpr reverse_iterator rbegin() noexcept { return reverse_iterator{end()}; }
		constexpr reverse_iterator rend() noexcept { return reverse_iterator{begin()}; }
		constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator{end()}; }
		constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator{begin()}; }
		constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator{cend()}; }
		constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator{cbegin()}; }

		constexpr reference operator[](size_type const ix) noexcept { return inner_.data_[ix]; }
		constexpr const_reference operator[](size_type const ix) const noexcept { return inner_.data_[ix]; }

		constexpr auto operator<=>(flex_array const &other) const noexcept = default;

		friend constexpr void swap(flex_array &lhs, flex_array &rhs) noexcept {
			std::swap(lhs.inner_, rhs.inner_);
		}
	};

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_FLEXARRAY_HPP
