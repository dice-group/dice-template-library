#ifndef DICE_TEMPLATELIBRARY_FLEXARRAY_HPP
#define DICE_TEMPLATELIBRARY_FLEXARRAY_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <span>
#include <stdexcept>

#include <ankerl/svector.h>

namespace dice::template_library {
    using std::dynamic_extent;

	namespace detail_flex_array {
		template<typename T, size_t extent, size_t max_extent>
		struct flex_array_inner;


		template<typename T, size_t extent> requires (extent != dynamic_extent)
		struct flex_array_inner<T, extent, extent> {
			// fully fixed size

			static constexpr size_t size_ = extent;
			std::array<T, extent> data_;

			static constexpr size_t size() noexcept {
				return size_;
			}

			operator std::span<T, extent>() noexcept {
				return data_;
			}

			operator std::span<T const, extent>() const noexcept {
				return data_;
			}

			constexpr auto operator<=>(flex_array_inner const &) const noexcept = default;
		};

		template<typename T, size_t max_extent>
		struct flex_array_inner<T, dynamic_extent, max_extent> {
			// fixed max size, dynamic actual size

			size_t size_ = 0;
			std::array<T, max_extent> data_;

			[[nodiscard]] constexpr size_t size() const noexcept {
				return size_;
			}

			constexpr void set_size(size_t size) noexcept {
				size_ = size;
			}

			operator std::span<T>() noexcept {
				return {data_.data(), size_};
			}

			operator std::span<T const>() const noexcept {
				return {data_.data(), size_};
			}

			constexpr std::pair<std::span<T const>, std::span<T const>> to_spans(flex_array_inner const &other) const noexcept {
				return {std::span<T const>{data_.data(), size_},
						std::span<T const>{other.data_.data(), other.size_}};
			}

			template<typename Cmp>
			constexpr auto lex_compare_impl(flex_array_inner const &other) const noexcept {
				auto const [self_s, other_s] = to_spans(other);
				return std::ranges::lexicographical_compare(self_s, other_s, Cmp{});
			}

			template<typename Cmp>
			constexpr auto eq_compare_impl(flex_array_inner const &other) const noexcept {
				auto const [self_s, other_s] = to_spans(other);
				return std::ranges::equal(self_s, other_s, Cmp{});
			}

			// operator <=> is not defaulted
			// so we need to provide all comparison operators manually

			constexpr auto operator<=>(flex_array_inner const &other) const noexcept requires requires (T x) { x <=> x; } {
				auto const [self_s, other_s] = to_spans(other);
				return std::lexicographical_compare_three_way(self_s.begin(), self_s.end(), other_s.begin(), other_s.end());
			}

			constexpr bool operator==(flex_array_inner const &other) const noexcept requires requires (T x) { x == x; } {
				return eq_compare_impl<std::equal_to<T>>(other);
			}

			constexpr bool operator!=(flex_array_inner const &other) const noexcept requires requires (T x) { x != x; } {
				return eq_compare_impl<std::not_equal_to<T>>(other);
			}

			constexpr bool operator<(flex_array_inner const &other) const noexcept requires requires (T x) { x < x; } {
				return lex_compare_impl<std::less<T>>(other);
			}

			constexpr bool operator<=(flex_array_inner const &other) const noexcept requires requires (T x) { x <= x; } {
				return lex_compare_impl<std::less_equal<T>>(other);
			}

			constexpr bool operator>(flex_array_inner const &other) const noexcept requires requires (T x) { x > x; } {
				return lex_compare_impl<std::greater<T>>(other);
			}

			constexpr bool operator>=(flex_array_inner const &other) const noexcept requires requires (T x) { x >= x; } {
				return lex_compare_impl<std::greater_equal<T>>(other);
			}
		};

		template<typename T, size_t extent> requires (extent != dynamic_extent)
		struct flex_array_inner<T, extent, dynamic_extent> {
			// dynamic max size, fixed small buffer size

			::ankerl::svector<T, extent> data_;

			[[nodiscard]] size_t size() const noexcept {
				return data_.size();
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

			constexpr auto operator<=>(flex_array_inner const &other) const noexcept = default;
		};

	} // namespace detail_flex_array

	/**
	 * A combination of std::array and std::span.
	 * If extent_ is set to some integer value (!= dynamic_extent), flex_array behaves like std::array, i.e. has a fixed, statically known size, max_size/capacity.
	 * If extent_ is set to dynamic_extent, max_extent_ must be set to some integer value (!= dynamic_extent). In this case
	 * flex_array behaves similar to static_vector, i.e. it is a collection with a fixed, statically known max_size/capacity while the
	 * actual size is a runtime value.
	 *
	 * @tparam T value type
	 * @tparam extent_ extent of the flex array
	 * @tparam max_extent_ max extent of the flex array
	 */
	template<typename T, size_t extent_, size_t max_extent_ = extent_>
	struct flex_array {
		// extent_ != dynamic_extent -> extent_ == max_extent_
		static_assert(extent_ == dynamic_extent || extent_ == max_extent_,
					  "If extent is not dynamic_extent, extent must be equal to max_extent");

		// extent_ == dynamic_extent -> max_extent_ != dynamic_extent
		static_assert(extent_ != dynamic_extent || max_extent_ != dynamic_extent,
					  "If extent is dynamic_extent, max_extent must not be dynamic_extent");

		// max_extent_ == dynamic_extent -> extend_ == dynamic_extent
		static_assert(max_extent_ != dynamic_extent || extent_ == dynamic_extent,
					  "If max_extent is dynamic_extent, extent must not be dynamic_extent");

		static constexpr size_t extent = extent_;
		static constexpr size_t max_extent = max_extent_;

		static constexpr bool has_max_extent = max_extent != dynamic_extent;
		static constexpr bool has_dynamic_extent = extent == dynamic_extent || max_extent == dynamic_extent;

	private:
		using inner_type = detail_flex_array::flex_array_inner<T, extent, max_extent>;

	public:
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
			size_t ix = 0;
			while (first != last) {
				if constexpr (has_max_extent) {
					if (ix >= max_size()) [[unlikely]] {
						throw std::length_error{"flex_array::flex_array: maximum size exceeded"};
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
		template<size_t other_max_extent>
		explicit constexpr flex_array(flex_array<value_type, dynamic_extent, other_max_extent> const &other) requires (!has_dynamic_extent) {
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

			inner_.size_ = other.size();
			std::ranges::copy(other, begin());
		}

		constexpr operator std::span<value_type, has_dynamic_extent ? dynamic_extent : extent>() noexcept {
			return inner_;
		}

		constexpr operator std::span<value_type const, has_dynamic_extent ? dynamic_extent : extent>() const noexcept {
			return inner_;
		}

		[[nodiscard]] static constexpr size_type max_size() noexcept { return max_extent; }
		[[nodiscard]] static constexpr size_type capacity() noexcept { return max_extent; }
		[[nodiscard]] constexpr size_type size() const noexcept { return inner_.size_; }
		[[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

		void resize(size_type new_size) requires (has_dynamic_extent) {
			if constexpr (has_max_extent) {
				if (new_size > max_extent) [[unlikely]] {
					throw std::invalid_argument{"flex_array::resize: new_size exceeds max_extent"};
				}
			}

			inner_.set_size(new_size);
		}

		[[nodiscard]] constexpr pointer data() noexcept { return inner_.data_.data(); }
		[[nodiscard]] constexpr const_pointer data() const noexcept { return inner_.data_.data(); }

		constexpr iterator begin() noexcept { return inner_.data_.begin(); }
		constexpr iterator end() noexcept { return std::next(begin(), size()); }
		constexpr const_iterator begin() const noexcept { return inner_.data_.begin(); }
		constexpr const_iterator end() const noexcept { return std::next(begin(), size()); }
		constexpr const_iterator cbegin() const noexcept { return inner_.data_.cbegin(); }
		constexpr const_iterator cend() const noexcept { return std::next(cbegin(), size()); }
		constexpr reverse_iterator rbegin() noexcept { return inner_.data_.rbegin(); }
		constexpr reverse_iterator rend() noexcept { return std::next(rbegin(), size()); }
		constexpr const_reverse_iterator rbegin() const noexcept { return inner_.data_.rbegin(); }
		constexpr const_reverse_iterator rend() const noexcept { return std::next(rbegin(), size()); }
		constexpr const_reverse_iterator crbegin() const noexcept { return inner_.data_.crbegin(); }
		constexpr const_reverse_iterator crend() const noexcept { return std::next(crbegin(), size()); }

		constexpr reference operator[](size_type const ix) noexcept { return inner_.data_[ix]; }
		constexpr const_reference operator[](size_type const ix) const noexcept { return inner_.data_[ix]; }

		constexpr auto operator<=>(flex_array const &other) const noexcept = default;

		friend constexpr void swap(flex_array &lhs, flex_array &rhs) noexcept {
			std::swap(lhs.inner_, rhs.inner_);
		}
	};

} // namespace dice::template_library

#endif // DICE_TEMPLATELIBRARY_FLEXARRAY_HPP
