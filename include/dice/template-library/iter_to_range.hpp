#ifndef DICE_TEMPLATELIBRARY_ITERTORANGE_HPP
#define DICE_TEMPLATELIBRARY_ITERTORANGE_HPP

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	/**
	 * Rust inspired iterator concept (see https://doc.rust-lang.org/std/iter/trait.Iterator.html for comparison)
	 */
	template<typename Iter>
	concept rust_style_iterator = requires (Iter iter) {
		typename Iter::value_type;
		{ iter.next() } -> std::same_as<std::optional<typename Iter::value_type>>;
	};

	/**
	 * Make a C++-style range out of a rust-style iterator.
	 * This is meant to save you from writing all the boilerplate that is required for C++ ranges and iterators.
	 *
	 * @tparam Iter inner iterator
	 */
	template<rust_style_iterator Iter>
	struct iter_to_range {
		using value_type = typename Iter::value_type;
		using sentinel = std::default_sentinel_t;

		struct iterator {
			using inner_iterator = Iter;
			using value_type = value_type;
			using reference = value_type const &;
			using pointer = value_type const *;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::input_iterator_tag;

		private:
			Iter inner_;
			std::optional<value_type> cur_;
			std::optional<value_type> peeked_;

			void advance() {
				if (peeked_.has_value()) [[unlikely]] {
					// fast path, we have already peaked the value
					cur_ = std::exchange(peeked_, std::nullopt);
				} else {
					cur_ = inner_.next();
				}
			}

		public:
			template<typename ...Args>
			explicit iterator(Args &&...args) : inner_{std::forward<Args>(args)...},
												cur_{inner_.next()} {
			}

			[[nodiscard]] inner_iterator const &inner() const noexcept {
				return inner_;
			}

			[[nodiscard]] inner_iterator &inner() noexcept {
				return inner_;
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

			iterator &operator++() {
				advance();
				return *this;
			}

			std::conditional_t<std::is_copy_constructible_v<Iter>, iterator, void> operator++(int) {
				if constexpr (std::is_copy_constructible_v<Iter>) {
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
					peeked_ = inner_.next();
				}

				return peeked_;
			}

			friend bool operator==(iterator const &self, sentinel) noexcept {
				return !self.cur_.has_value();
			}

			friend bool operator==(sentinel, iterator const &self) noexcept {
				return !self.cur_.has_value();
			}
		};

	private:
		std::conditional_t<
			std::is_copy_constructible_v<Iter>,
			Iter,
			std::function<iterator()>
		> make_iter_;

	public:
		template<typename ...Args> requires (!std::is_copy_constructible_v<Iter>)
		explicit iter_to_range(Args &&...args)
			: make_iter_{[...args = std::forward<Args>(args)] { return iterator{args...}; }} {
		}

		template<typename ...Args> requires (std::is_copy_constructible_v<Iter>)
		explicit iter_to_range(Args &&...args)
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
				return make_iter_();
			}
		}

		static constexpr sentinel end() noexcept {
			return std::default_sentinel;
		}
	};

} // namespace dice::template_library


#endif // DICE_TEMPLATELIBRARY_ITERTORANGE_HPP
