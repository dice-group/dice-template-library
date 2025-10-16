#ifndef DICE_TEMPLATELIBRARY_ITERTORANGE_HPP
#define DICE_TEMPLATELIBRARY_ITERTORANGE_HPP

#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	/**
	 * Wrapper to make a C++-style iterator out of a rust-style iterator.
	 *
	 * @tparam Iter base rust-style iterator
	 *
	 * Requirements for Iter (the following things must be at least protected):
	 *		typename Iter::value_type;
	 *		{ Iter::next() } -> std::same_as<std::optional<typename Iter::value_type>>;
	 *
	 * We are not using a concept here, because concepts require things to be public
	 */
	template<typename Iter>
	struct iter_to_iter : Iter {
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

		void advance() {
			if (peeked_.has_value()) [[unlikely]] {
				// fast path, we have already peaked the value
				cur_ = std::exchange(peeked_, std::nullopt);
			} else {
				cur_ = this->next();
			}
		}

	public:
		template<typename ...Args>
		explicit iter_to_iter(Args &&...args) : Iter{std::forward<Args>(args)...},
												cur_{this->next()} {
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

		iter_to_iter &operator++() {
			advance();
			return *this;
		}

		std::conditional_t<std::is_copy_constructible_v<base_iterator>, iter_to_iter, void> operator++(int) {
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
				peeked_ = this->next();
			}

			return peeked_;
		}

		friend bool operator==(iter_to_iter const &self, sentinel) noexcept {
			return !self.cur_.has_value();
		}

		friend bool operator==(sentinel, iter_to_iter const &self) noexcept {
			return !self.cur_.has_value();
		}
	};

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
	template<typename Iter>
	struct iter_to_range {
		using iterator = iter_to_iter<Iter>;
		using sentinel = typename iterator::sentinel;
		using value_type = typename iterator::value_type;

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
			return sentinel{};
		}
	};

} // namespace dice::template_library


#endif // DICE_TEMPLATELIBRARY_ITERTORANGE_HPP
