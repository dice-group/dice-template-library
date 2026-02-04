#ifndef DICE_TEMPLATELIBRARY_NEXTTORANGE_HPP
#define DICE_TEMPLATELIBRARY_NEXTTORANGE_HPP

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace dice::template_library {
	/**
	 * A rust-style forward iterator with next() that consumes elements from the front.
	 */
	template<typename I>
	concept next_iterator = requires (I &iter) {
		typename I::value_type;
		{ iter.next() } -> std::same_as<std::optional<typename I::value_type>>;
	};

	/**
	 * A rust-style backwards iterator with a next_back() function that consumes elements from the back.
	 */
	template<typename I>
	concept next_back_iterator = requires (I &iter) {
		typename I::value_type;
		{ iter.next_back() } -> std::same_as<std::optional<typename I::value_type>>;
	};

	/**
	 * A rust-style iterator that knows how many elements it has left.
	 */
	template<typename I>
	concept sized_next_iterator = requires (I const &iter) {
		{ iter.remaining() } -> std::convertible_to<size_t>;
	};

	/**
	 * A rust-style forward iterator with nth(off) that consumes elements from the front.
	 */
	template<typename I>
	concept nth_iterator = requires (I &iter, size_t off) {
		typename I::value_type;
		{ iter.nth(off) } -> std::same_as<std::optional<typename I::value_type>>;
	};

	/**
	 * A rust-style backwards iterator with nth_back(off) that consumes elements from the front.
	 *
	 * Requirements for I (the following things must be at least protected):
	 *		{ I::nth_back(off) } -> std::same_as<std::optional<typename I::value_type>>;
	 */
	template<typename I>
	concept nth_back_iterator = requires (I &iter, size_t off) {
		typename I::value_type;
		{ iter.nth_back(off) } -> std::same_as<std::optional<typename I::value_type>>;
	};


	namespace detail_next_to_iter {
		enum struct direction : bool {
			forward,
			backward,
		};

		/**
		 * @tparam Iter the underlying (rust-style) iterator
		 * @tparam dir the walking direction of the iterator (forward uses next(), backward uses next_back())
		 */
		template<typename Iter, direction dir> requires (dir == direction::forward ? next_iterator<Iter> : next_back_iterator<Iter>)
		struct next_to_iter_impl : Iter {
			using base_iterator = Iter;
			using sentinel = std::default_sentinel_t;
			using value_type = typename Iter::value_type;
			using reference = value_type const &;
			using pointer = value_type const *;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::input_iterator_tag;

		private:
			static constexpr bool efficient_skip = dir == direction::forward ? nth_iterator<Iter> : nth_back_iterator<Iter>;

			std::optional<value_type> cur_;
			std::optional<value_type> peeked_;

			[[nodiscard]] std::optional<value_type> next() {
				if constexpr (dir == direction::forward) {
					return Iter::next();
				} else {
					return Iter::next_back();
				}
			}

			[[nodiscard]] std::optional<value_type> nth(size_t off) requires (efficient_skip) {
				if constexpr (dir == direction::forward) {
					return Iter::nth(off);
				} else {
					return Iter::nth_back(off);
				}
			}

			void advance() {
				if (peeked_.has_value()) [[unlikely]] {
					// fast path, we have already peaked the value
					cur_ = std::exchange(peeked_, std::nullopt);
				} else {
					cur_ = next();
				}
			}

			void advance_by(size_t off) {
				if (off == 0) {
					return;
				}

				// "nth" advances behind the nth element. See below.
				off -= 1;

				if (peeked_.has_value()) [[unlikely]] {
					// we have already peeked the value
					if (off == 0) {
						cur_ = std::exchange(peeked_, std::nullopt);
					} else {
						peeked_.reset(); // discard peeked value
						cur_ = nth(off - 1);
					}
				} else {
					cur_ = nth(off);
				}
			}

		public:
			template<typename ...Args>
			explicit next_to_iter_impl(Args &&...args) : Iter{std::forward<Args>(args)...},
														 cur_{next()} {
			}

			[[nodiscard]] reference operator*() const noexcept {
				assert(cur_.has_value());
				return *cur_;
			}

			[[nodiscard]] pointer operator->() const noexcept {
				assert(cur_.has_value());
				return &*cur_;
			}

			/**
			 * Access the current value of the iterator mutably.
			 * This is required because in ranges, iterators must have the same reference type for const and non-const access of operator*.
			 * This means a non-const overload of operator* that returns a non-const reference is not allowed.
			 *
			 * @return reference to current value
			 */
			[[nodiscard]] value_type &value() noexcept {
				assert(cur_.has_value());
				return *cur_;
			}

			[[nodiscard]] value_type const &value() const noexcept {
				assert(cur_.has_value());
				return *cur_;
			}

			next_to_iter_impl &operator++() {
				advance();
				return *this;
			}

			std::conditional_t<std::is_copy_constructible_v<base_iterator>, next_to_iter_impl, void> operator++(int) {
				if constexpr (std::is_copy_constructible_v<base_iterator>) {
					auto cpy = *this;
					advance();
					return cpy;
				} else {
					advance();
				}
			}

			next_to_iter_impl &operator+=(size_t off) requires (efficient_skip) {
				advance_by(off);
				return *this;
			}

			next_to_iter_impl operator+(size_t off) const requires (efficient_skip && std::is_copy_constructible_v<base_iterator>) {
				auto cpy = *this;
				cpy += off;
				return cpy;
			}

			/**
			 * Takes a peek at the next element if there is one, but does not advance the iterator onto it.
			 * This function is meant to be used when the underlying iterator is not copyable or expensive to copy.
			 *
			 * @return nullopt if there is no next element, the element if there is a next element
			 */
			[[nodiscard]] std::optional<value_type> const &peek() {
				if (!peeked_.has_value()) {
					peeked_ = next();
				}

				return peeked_;
			}

			friend bool operator==(next_to_iter_impl const &self, sentinel) noexcept {
				return !self.cur_.has_value();
			}

			friend bool operator==(sentinel, next_to_iter_impl const &self) noexcept {
				return !self.cur_.has_value();
			}
		};

	} // namespace detail_next_to_iter

	/**
	 * Wrapper to make a C++-style iterator out of a rust-style iterator.
	 *
	 * @tparam Iter base rust-style iterator
	 *
	 * Requirements for Iter:
	 *   - value_type (required):
	 *		// The value type of the iterator.
	 *		typename Iter::value_type;
	 *   - next() (required):
	 *		// Return the next element of the iterator.
	 *		{ iter.next() } -> std::optional<typename Iter::value_type>;
	 *   - remaining() (optional):
	 *		// Return the number of elements currently remaining in the iterator.
	 *		// Enables sized_range for next_to_range if Iter is copy-constructible.
	 *		{ iter.remaining() } -> std::convertible_to<size_t>;
	 *   - nth(off) (optional):
	 *		// Return the nth element of the iterator and advance the iterator behind it.
	 *		// Enables operator+= (and operator+ if Iter is copy-constructible)
	 *		{ iter.nth(size_t{off}) } -> std::optional<typename Iter::value_type>;
	 */
	template<next_iterator Iter>
	using next_to_iter = detail_next_to_iter::next_to_iter_impl<Iter, detail_next_to_iter::direction::forward>;

	/**
	 * Wrapper to make a C++-style iterator out of a rust-style reverse iterator.
	 *
	 * @tparam Iter base rust-style reverse iterator
	 *
	 * Requirements for Iter:
	 *   - value_type (required):
	 *		// The value type of the iterator.
	 *		typename Iter::value_type;
	 *   - next_back() (required):
	 *		// Return the next element from the back of the iterator.
	 *		{ iter.next_back() } -> std::optional<typename Iter::value_type>;
	 *   - remaining() (optional):
	 *		// Return the number of elements currently remaining in the iterator.
	 *		// Enables sized_range for next_to_range if Iter is copy-constructible.
	 *		{ iter.remaining() } -> std::convertible_to<size_t>;
	 *   - nth_back(off) (optional):
	 *		// Return the nth element from the back of the iterator and advance the iterator before it.
	 *		// Enables operator+= (and operator+ if Iter is copy-constructible)
	 *		{ iter.nth_back(size_t{off}) } -> std::optional<typename Iter::value_type>;
	 */
	template<next_back_iterator Iter>
	using next_back_to_reverse_iter = detail_next_to_iter::next_to_iter_impl<Iter, detail_next_to_iter::direction::backward>;


	/**
	 * Make a C++-style range out of a rust-style iterator.
	 * This is meant to save you from writing all the boilerplate that is required for C++ ranges and iterators.
	 *
	 * @tparam Iter base rust-style iterator
	 *
	 * Requirements for Iter:
	 *  Input Iterator:
	 *   - value_type (required):
	 *		// The value type of the iterator.
	 *		typename Iter::value_type;
	 *   - next() (required):
	 *		// Return the next element of the iterator.
	 *		{ iter.next() } -> std::optional<typename Iter::value_type>;
	*	 - nth(off) (optional):
	 *		// Return the nth element of the iterator and advance the iterator behind it.
	 *		// Enables operator+= (and operator+ if Iter is copy-constructible) on the iterator.
	 *		{ iter.nth(size_t{off}) } -> std::optional<typename Iter::value_type>;
	 *
	 *  Reverse Input Iterator:
	 *   - next_back() (optional):
	 *		// Return the next element from the back of the iterator.
	 *		// Enables rbegin, rend and reverse on next_to_range.
	 *		{ iter.next_back() } -> std::optional<typename Iter::value_type>;
	 *   - nth_back(off) (optional):
	 *		// Return the nth element from the back of the iterator and advance the iterator before it.
	 *		// Enables operator+= (and operator+ if Iter is copy-constructible) on the reverse iterator.
	 *		{ iter.nth_back(size_t{off}) } -> std::optional<typename Iter::value_type>;
	 *
	 *  Sized Range:
	 *	 - remaining() (optional):
	 *		// Return the number of elements currently remaining in the iterator.
	 *		// Enables sized_range for next_to_range if Iter is copy-constructible.
	 *		{ iter.remaining() } -> std::convertible_to<size_t>;
	 */
	template<next_iterator Iter>
	struct next_to_range {
		using iterator = next_to_iter<Iter>;
		using sentinel = typename iterator::sentinel;
		using value_type = typename iterator::value_type;

	private:
		std::conditional_t<
			std::is_copy_constructible_v<Iter>,
			Iter,
			std::function<Iter()>
		> make_iter_;

		[[nodiscard]] Iter iter() const {
			if constexpr (std::is_copy_constructible_v<Iter>) {
				return make_iter_;
			} else {
				return make_iter_();
			}
		}

	public:
		template<typename ...Args> requires (!std::is_copy_constructible_v<Iter>)
		explicit next_to_range(Args &&...args)
			: make_iter_{[...args = std::forward<Args>(args)] { return Iter{args...}; }} {
		}

		template<typename ...Args> requires (std::is_copy_constructible_v<Iter>)
		explicit next_to_range(Args &&...args)
			: make_iter_{std::forward<Args>(args)...} {
		}

		/**
		 * @return a new iterator from the beginning of the range
		 * @note this *always* returns a new iterator, regardless if there are other iterators alive
		 */
		[[nodiscard]] iterator begin() const {
			return iterator{iter()};
		}

		[[nodiscard]] iterator cbegin() const {
			return begin();
		}

		static constexpr sentinel end() noexcept {
			return sentinel{};
		}

		static constexpr sentinel cend() noexcept {
			return end();
		}

		/**
		 * @return a new iterator from the end of the range
		 * @note this *always* returns a new iterator, regardless if there are other iterators alive
		 */
		[[nodiscard]] auto rbegin() const requires (next_back_iterator<Iter>) {
			return next_back_to_reverse_iter<Iter>{iter()};
		}

		[[nodiscard]] auto crbegin() const requires (nth_back_iterator<Iter>) {
			return rbegin();
		}

		static constexpr sentinel rend() noexcept requires (next_back_iterator<Iter>) {
			return sentinel{};
		}

		static constexpr sentinel crend() noexcept requires (next_back_iterator<Iter>) {
			return end();
		}

		/**
		 * @return a reversed view of *this
		 * @note this exists because std::views::reverse requires `std::bidirectional_iterator` from the iterator
		 */
		[[nodiscard]] auto reversed() const requires (next_back_iterator<Iter>) {
			return std::ranges::subrange(rbegin(), rend());
		}

		/**
		 * @return the first element of the range
		 */
		[[nodiscard]] value_type front() const requires (std::is_copy_constructible_v<Iter>) {
			return *auto{make_iter_}.next();
		}

		/**
		 * @return the last element of the range
		 */
		[[nodiscard]] value_type back() const requires (std::is_copy_constructible_v<Iter> && next_back_iterator<Iter>) {
			return *auto{make_iter_}.next_back();
		}

		/**
		 * @param off element index
		 * @return the element at the given position
		 */
		[[nodiscard]] value_type operator[](size_t off) const requires (std::is_copy_constructible_v<Iter> && nth_iterator<Iter>) {
			return *auto{make_iter_}.nth(off);
		}

		/**
		 * @return the size of the range
		 */
		[[nodiscard]] size_t size() const noexcept requires (std::is_copy_constructible_v<Iter> && sized_next_iterator<Iter>) {
			return make_iter_.remaining();
		}

		/**
		 * @return true iff the range is empty
		 */
		[[nodiscard]] bool empty() const noexcept requires (std::is_copy_constructible_v<Iter> && sized_next_iterator<Iter>) {
			return size() == 0;
		}

		/**
		 * @return true iff the range is non-empty
		 */
		[[nodiscard]] explicit operator bool() const noexcept requires (std::is_copy_constructible_v<Iter> && sized_next_iterator<Iter>) {
			return !empty();
		}

		/**
		 * Similar to std::ranges::subrange::advance, drops the first n elements off the start of the range.
		 *
		 * @note does not support re-adding the elements with negative offsets.
		 * @param off number of elements to drop
		 * @return *this
		 */
		next_to_range &advance(size_t off) requires (std::is_copy_constructible_v<Iter> && nth_iterator<Iter>) {
			if (off != 0) {
				// "nth" advances behind the nth element
				off -= 1;
				(void) make_iter_.nth(off);
			}

			return *this;
		}
	};

} // namespace dice::template_library


#endif // DICE_TEMPLATELIBRARY_NEXTTORANGE_HPP
