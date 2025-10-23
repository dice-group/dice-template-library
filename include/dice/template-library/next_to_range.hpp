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

	namespace detail_next_to_iter {

		template<typename T>
		struct detect_next : std::false_type {
		};

		template<typename T> requires (std::is_class_v<T>)
		struct detect_next<T> : T {
			static constexpr bool value = requires {
				typename T::value_type;
				{ std::declval<detect_next>().next() } -> std::same_as<std::optional<typename T::value_type>>;
			};
		};

		template<typename T>
		struct detect_next_back : std::false_type {
		};

		template<typename T> requires (std::is_class_v<T>)
		struct detect_next_back<T> : T {
			static constexpr bool value = requires {
				typename T::value_type;
				{ std::declval<detect_next_back>().next_back() } -> std::same_as<std::optional<typename T::value_type>>;
			};
		};

		template<typename Iter, bool reverse>
		struct next_to_iter_impl : Iter {
			using base_iterator = Iter;
			using sentinel = std::default_sentinel_t;
			using value_type = typename Iter::value_type;
			using reference = value_type const &;
			using pointer = value_type const *;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::input_iterator_tag;

		private:
			std::optional<value_type> cur_;
			std::optional<value_type> peeked_;

			[[nodiscard]] std::optional<value_type> next() {
				if constexpr (reverse) {
					return Iter::next_back();
				} else {
					return Iter::next();
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
	 * A rust-style forward iterator.
	 *
 	 * Requirements for I (the following things must be at least protected):
	 *		typename I::value_type;
	 *		{ I::next() } -> std::same_as<std::optional<typename I::value_type>>;
	 */
	template<typename I>
	concept next_iterator = detail_next_to_iter::detect_next<I>::value;

	/**
	 * A rust-style backwards iterator.
	 *
	 * Requirements for I (the following things must be at least protected):
	 *		typename I::value_type;
	 *		{ I::next_back() } -> std::same_as<std::optional<typename I::value_type>>;
	 */
	template<typename I>
	concept next_back_iterator = detail_next_to_iter::detect_next_back<I>::value;

	/**
	 * A rust-style iterator that knows how many elements it has left.
	 */
	template<typename I>
	concept sized_next_iterator = requires (I const &iter) {
		{ iter.remaining() } -> std::convertible_to<size_t>;
	};


	/**
	 * Wrapper to make a C++-style iterator out of a rust-style iterator.
	 *
	 * @tparam Iter base rust-style iterator
	 */
	template<next_iterator Iter>
	using next_to_iter = detail_next_to_iter::next_to_iter_impl<Iter, false>;

	/**
	 * Wrapper to make a C++-style iterator out of a rust-style reverse iterator.
	 *
	 * @tparam Iter base rust-style reverse iterator
	 */
	template<next_back_iterator Iter>
	using next_back_to_reverse_iter = detail_next_to_iter::next_to_iter_impl<Iter, true>;


	/**
	 * Make a C++-style range out of a rust-style iterator.
	 * This is meant to save you from writing all the boilerplate that is required for C++ ranges and iterators.
	 *
	 * @tparam Iter base rust-style iterator
	 *
	 * Requirements for Iter (the following things must be at least protected):
	 *		typename Iter::value_type;
	 *		{ Iter::next() } -> std::same_as<std::optional<typename Iter::value_type>>;
	 *
	 * We are not using a concept here, because concepts require things to be public
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
			if constexpr (std::is_copy_constructible_v<Iter>) {
				return iterator{make_iter_};
			} else {
				return iterator{make_iter_()};
			}
		}

		static constexpr sentinel end() noexcept {
			return sentinel{};
		}

		[[nodiscard]] auto rbegin() const requires (next_back_iterator<Iter>) {
			if constexpr (std::is_copy_constructible_v<Iter>) {
				return next_back_to_reverse_iter<Iter>{make_iter_};
			} else {
				return next_back_to_reverse_iter<Iter>{make_iter_()};
			}
		}

		static constexpr sentinel rend() noexcept {
			return sentinel{};
		}

		[[nodiscard]] auto reversed() const requires (next_back_iterator<Iter>) {
			return std::ranges::subrange(rbegin(), rend());
		}

		[[nodiscard]] size_t size() const noexcept requires (std::is_copy_constructible_v<Iter> && sized_next_iterator<Iter>) {
			return make_iter_.remaining();
		}
	};

} // namespace dice::template_library


#endif // DICE_TEMPLATELIBRARY_NEXTTORANGE_HPP
