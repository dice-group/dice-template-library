#ifndef DICE_TEMPLATELIBRARY_HPP
#define DICE_TEMPLATELIBRARY_HPP

#include <algorithm>
#include <functional>
#include <ranges>
#include <set>
#include <unordered_set>

// all_of, any_of, none_of terminal algorithm
#define DTL_DEFINE_PIPEABLE_RANGE_ALGO(ALGO_NAME)                                     \
	namespace ranges_algo_detail {                                                    \
		template<typename Pred>                                                       \
		struct ALGO_NAME##_fn {                                                       \
			Pred pred;                                                                \
                                                                                      \
			template<typename P>                                                      \
			constexpr explicit ALGO_NAME##_fn(P &&p) : pred(std::forward<P>(p)) {}    \
                                                                                      \
			/* input_range causes no dangling risk since this adaptor is terminal  */ \
			template<std::ranges::input_range R>                                      \
			constexpr bool                                                            \
			operator()(R &&r) const {                                                 \
				return std::ranges::ALGO_NAME(r, pred);                               \
			}                                                                         \
                                                                                      \
			template<typename R>                                                      \
			friend auto operator|(R &&r, const ALGO_NAME##_fn &self)                  \
					-> decltype(self(std::forward<R>(r))) {                           \
				return self(std::forward<R>(r));                                      \
			}                                                                         \
		};                                                                            \
	}                                                                                 \
                                                                                      \
	template<typename Pred>                                                           \
	constexpr auto ALGO_NAME(Pred &&pred) {                                           \
		return ranges_algo_detail::ALGO_NAME##_fn<std::decay_t<Pred>>(                \
				std::forward<Pred>(pred));                                            \
	}

namespace dice::template_library {
	DTL_DEFINE_PIPEABLE_RANGE_ALGO(all_of)
	DTL_DEFINE_PIPEABLE_RANGE_ALGO(any_of)
	DTL_DEFINE_PIPEABLE_RANGE_ALGO(none_of)
}// namespace dice::template_library

// empty / non_empty terminal algorithm
namespace dice::template_library {
	namespace ranges_algo_detail {
		struct empty_fn {
			template<std::ranges::input_range R>
			constexpr bool operator()(R &&range) const {
				// Use std::ranges::empty if the expression is valid for the range type.
				if constexpr (requires { std::ranges::empty(range); }) {
					return std::ranges::empty(range);
				} else {
					// std::ranges::begin/end work for const and non-const ranges.
					return std::ranges::begin(range) == std::ranges::end(range);
				}
			}

			template<std::ranges::input_range R>
			friend constexpr auto operator|(R &&range, const empty_fn &self)
					-> decltype(self(std::forward<R>(range))) {
				return self(std::forward<R>(range));
			}
		};

		struct non_empty_fn {
			template<std::ranges::input_range R>
			constexpr bool operator()(R &&range) const {
				// Implemented as the negation of empty_fn for code reuse.
				return !empty_fn{}(std::forward<R>(range));
			}

			template<std::ranges::input_range R>
			friend constexpr auto operator|(R &&range, const non_empty_fn &self)
					-> decltype(self(std::forward<R>(range))) {
				return self(std::forward<R>(range));
			}
		};
	}// namespace ranges_algo_detail

	/**
     * @brief Checks if a range is empty.
     *
     * This is a pipeable, terminal algorithm. It uses `std::ranges::empty` if available,
     * otherwise it falls back to comparing `cbegin()` and `cend()`.
     *
     * @return A closure that returns true if the range is empty otherwise false.
     */
	inline constexpr ranges_algo_detail::empty_fn empty{};

	/**
     * @brief Checks if a range is not empty.
     *
     * This is a pipeable, terminal algorithm and the logical negation of `dtl::empty`.
     *
     * @return A closure that returns true if the range has at least one element otherwise false.
     */
	inline constexpr ranges_algo_detail::non_empty_fn non_empty{};

}// namespace dice::template_library

// remove_element adaptor
namespace dice::template_library {
	namespace ranges_algo_detail {
		template<typename T, typename Pred>
		struct remove_element_fn {
			T value_;
			Pred pred_;

			template<typename V, typename P>
			constexpr remove_element_fn(V &&value, P &&pred)
				: value_(std::forward<V>(value)), pred_(std::forward<P>(pred)) {}

			template<std::ranges::viewable_range R>// viewable_range ensures there are no views into dangling stuff
			constexpr auto operator()(R &&range) const {
				return std::forward<R>(range) | std::views::filter([value = value_, pred = pred_](const auto &element) {
						   return !std::invoke(pred, element, value);
					   });
			}

			template<std::ranges::viewable_range R>// viewable_range ensures there are no views into dangling stuff
			friend constexpr auto operator|(R &&range, const remove_element_fn &self)
					-> decltype(self(std::forward<R>(range))) {
				return self(std::forward<R>(range));
			}
		};
	}// namespace ranges_algo_detail

	/**
	 * Creates a lazy, pipeable view of a range that excludes elements matching a given value. This adaptor does not modify the source range.
	 *
	 * @param value The value to filter out of the source range.
	 * @param pred Optional binary predicate for comparison. Defaults to `std::ranges::equal_to`.
	 * @return A range adaptor closure.
	 */
	template<typename T, typename Pred = std::ranges::equal_to>
	constexpr auto remove_element(T &&value, Pred &&pred = Pred{}) {
		return ranges_algo_detail::remove_element_fn<std::decay_t<T>, std::decay_t<Pred>>(
				std::forward<T>(value), std::forward<Pred>(pred));
	}
}// namespace dice::template_library

// all_equal adaptor
namespace dice::template_library {
	namespace ranges_algo_detail {
		template<typename Pred>
		struct all_equal_fn {
			Pred pred_;

			template<typename P>
			constexpr explicit all_equal_fn(P &&pred)
				: pred_(std::forward<P>(pred)) {}

			template<std::ranges::input_range R>
				requires std::copyable<std::ranges::range_value_t<R>>
			constexpr bool operator()(R &&range) const {
				if (dice::template_library::empty(range)) {
					return true;
				}

				auto it = std::ranges::begin(range);
				const auto first_element = *(it++);

				// Check if all subsequent elements in the range match the first one.
				return std::ranges::subrange(it, std::ranges::end(range)) |
					   dice::template_library::all_of([this, first_element](const auto &current_element) {
						   return std::invoke(pred_, current_element, first_element);
					   });
			}

			template<std::ranges::input_range R>
				requires std::copyable<std::ranges::range_value_t<R>>
			friend constexpr auto operator|(R &&range, const all_equal_fn &self)
					-> decltype(self(std::forward<R>(range))) {
				return self(std::forward<R>(range));
			}
		};
	}// namespace ranges_algo_detail

	/**
     * @brief Checks if all elements in a range are equal to each other.
     *
     * This is a pipeable, terminal algorithm that returns a boolean. Elements must be copyable for this to work.
     *
     * @param pred Optional binary predicate for comparison. Defaults to `std::ranges::equal_to`.
     * @return true if all elements in the range are equal, otherwise false.
     */
	template<typename Pred = std::ranges::equal_to>
	constexpr auto all_equal(Pred &&pred = Pred{}) {
		return ranges_algo_detail::all_equal_fn<std::decay_t<Pred>>(
				std::forward<Pred>(pred));
	}
}// namespace dice::template_library

// unique adaptor
namespace dice::template_library {
	namespace ranges_algo_detail {

		template<std::ranges::input_range V>
			requires std::ranges::view<V>
		struct unique_view : std::ranges::view_interface<unique_view<V>> {
		private:
			struct iterator;

			using ValueType = std::decay_t<std::ranges::range_value_t<V>>;
			static_assert(std::copy_constructible<ValueType>, "The value type for unique must be copy-constructible.");

			using SetType = std::conditional_t<
					// Check if std::hash is a valid expression for the value type.
					requires(const ValueType &v) { { std::hash<ValueType>{}(v) } -> std::convertible_to<std::size_t>; },
					std::unordered_set<ValueType>,
					std::set<ValueType>>;

			V base_{};
			// The set is mutable because it is populated during lazy iteration.
			mutable SetType seen_{};

		public:
			unique_view() = default;
			constexpr explicit unique_view(V base) : base_(std::move(base)) {}

			constexpr auto begin() {
				// Clear any previous state when starting a new iteration.
				seen_.clear();
				return iterator{this, std::ranges::begin(base_)};
			}

			constexpr auto end() const {
				return std::default_sentinel;
			}
		};

		template<std::ranges::input_range V>
			requires std::ranges::view<V>
		struct unique_view<V>::iterator {
		private:
			unique_view *parent_ = nullptr;
			std::ranges::iterator_t<V> current_{};

			void find_next_unique() {
				auto const end = std::ranges::end(parent_->base_);
				while (current_ != end) {
					if (parent_->seen_.insert(*current_).second) {
						break;
					}
					++current_;
				}
			}

		public:
			using iterator_concept = std::input_iterator_tag;
			using value_type = std::ranges::range_value_t<V>;
			using difference_type = std::ranges::range_difference_t<V>;

			iterator() = default;
			constexpr explicit iterator(unique_view *parent, std::ranges::iterator_t<V> current)
				: parent_(parent),
				  current_(std::move(current)) {
				find_next_unique();
			}

			constexpr auto operator*() const { return *current_; }

			constexpr iterator &operator++() {
				++current_;
				find_next_unique();
				return *this;
			}

			constexpr void operator++(int) { ++*this; }

			constexpr bool operator==(std::default_sentinel_t) const {
				return current_ == std::ranges::end(parent_->base_);
			}
		};

		struct unique_fn {
			template<std::ranges::viewable_range R>
			constexpr auto operator()(R &&r) const {
				return unique_view(std::views::all(std::forward<R>(r)));
			}

			template<std::ranges::viewable_range R>
			friend constexpr auto operator|(R &&r, const unique_fn &self)
					-> decltype(self(std::forward<R>(r))) {
				return self(std::forward<R>(r));
			}
		};

	}// namespace ranges_algo_detail

	/**
	 * @brief A range adaptor that produces a view containing the first occurrence of each element from an underlying range.
	 *
	 * This adaptor is lazy and preserves the relative order of the unique elements. It automatically uses a hash set
	 * for tracking if the element type is hashable, otherwise it falls back to a sorted set.
	 *
	 * @return A lazy view containing the unique elements.
	 */
	inline constexpr ranges_algo_detail::unique_fn unique{};

}// namespace dice::template_library

// range generator view (python-like iota with step)
namespace dice::template_library {
	namespace ranges_algo_detail {

		// The view that represents the generated sequence.
		template<std::integral T>
		struct range_generator_view : public std::ranges::view_interface<range_generator_view<T>> {
		private:
			class iterator;// Forward-declare

			T start_{};
			T stop_{};
			std::make_signed_t<T> step_{1};

		public:
			range_generator_view() = default;
			constexpr explicit range_generator_view(T start, T stop, std::make_signed_t<T> step)
				: start_(start),
				  stop_(stop),
				  step_(step) {}

			constexpr auto begin() const { return iterator{this, start_}; }
			constexpr auto end() const { return std::default_sentinel; }
		};

		// The iterator that generates the numbers on the fly.
		template<std::integral T>
		class range_generator_view<T>::iterator {
		private:
			const range_generator_view *parent_ = nullptr;
			T value_{};

		public:
			// Iterator traits to satisfy std::input_iterator
			using iterator_concept = std::input_iterator_tag;
			using value_type = T;
			using difference_type = std::ptrdiff_t;

			iterator() = default;
			constexpr explicit iterator(const range_generator_view *parent, T value)
				: parent_(parent),
				  value_(value) {}

			constexpr T operator*() const { return value_; }

			constexpr iterator &operator++() {
				value_ += parent_->step_;
				return *this;
			}
			constexpr void operator++(int) { ++*this; }

			constexpr bool operator==(std::default_sentinel_t) const {
				// The end is reached if the step is positive and value is >= stop,
				// or if the step is negative and value is <= stop.
				if (parent_->step_ > 0) {
					return value_ >= parent_->stop_;
				}
				return value_ <= parent_->stop_;
			}
		};
	}// namespace ranges_algo_detail

	/**
	 * @brief A lazy view that generates a sequence of numbers, similar to Python's range().
	 *
	 * This view can be used with one, two, or three arguments:
	 * - `range<T>(stop)`: Generates [0,stop).
	 * - `range<T>(start, stop)`: Generates [start,stop).
	 * - `range<T>(start, stop, step)`: Generates numbers from start, incrementing by step, until stop is met or passed.
	 */
	template<std::integral T, typename S = T>
	constexpr auto range(T arg1, T arg2, S step) {
		static_assert(std::is_integral_v<S>, "Step must be an integral type.");
		return ranges_algo_detail::range_generator_view<T>(arg1, arg2, static_cast<std::make_signed_t<T>>(step));
	}

	template<std::integral T>
	constexpr auto range(T start, T stop) {
		return ranges_algo_detail::range_generator_view<T>(start, stop, 1);
	}

	template<std::integral T>
	constexpr auto range(T stop) {
		return ranges_algo_detail::range_generator_view<T>(static_cast<T>(0), stop, 1);
	}

}// namespace dice::template_library

// all_distinct terminal algorithm
namespace dice::template_library {
	namespace ranges_algo_detail {
		template<typename Pred>
		struct all_distinct_fn {
			Pred pred_;

			template<typename P>
			constexpr explicit all_distinct_fn(P &&pred)
				: pred_(std::forward<P>(pred)) {}

			template<std::ranges::input_range R>
			constexpr bool operator()(R &&range) const {
				using ValueType = std::decay_t<std::ranges::range_value_t<R>>;

				if (dice::template_library::empty(range)) {
					return true;
				}

				// backend_selection
				constexpr bool is_default_pred = std::is_same_v<Pred, std::ranges::equal_to>;

				constexpr bool hash_set_usable = std::copy_constructible<ValueType> &&
												 requires(const ValueType &v) {
													 { std::hash<ValueType>{}(v) } -> std::convertible_to<std::size_t>;
												 };

				constexpr bool ordered_set_usable = std::copy_constructible<ValueType> &&
													requires { typename std::set<ValueType>; } &&
													is_default_pred;// non-default predicates cannot be used with the ordered set
				static_assert(hash_set_usable || ordered_set_usable, "The elements of the consumed range are neither compatible with std::unordered_set nor with std::set.");

				auto seen = [&] {
					if constexpr (hash_set_usable) {
						return std::unordered_set<ValueType, std::hash<ValueType>, Pred>{1, {}, pred_};
					} else {
						return std::set<ValueType>{};
					}
				}();
				for (const auto &element : range) {
					if (!seen.insert(element).second) { return false; }
				}
				return true;
			}

			template<std::ranges::input_range R>
			friend constexpr auto operator|(R &&range, const all_distinct_fn &self)
					-> decltype(self(std::forward<R>(range))) {
				return self(std::forward<R>(range));
			}
		};
	}// namespace ranges_algo_detail

	/**
	 * @brief Checks if all elements in a range are distinct from one another.
	 *
	 * This is a pipeable, terminal algorithm that returns a boolean.
	 * - When called without arguments, it uses a hash set if the type is hashable, otherwise it
	 * falls back to a sorted set if the type is comparable.
	 * - When called with a custom equality predicate, it requires the type to be hashable.
	 *
	 * @param pred Optional binary predicate for equality comparison. Defaults to `std::ranges::equal_to`.
	 * @return true if all elements are distinct, otherwise false.
	 */
	template<typename Pred = std::ranges::equal_to>
	constexpr auto all_distinct(Pred &&pred = Pred{}) {
		return ranges_algo_detail::all_distinct_fn<std::decay_t<Pred>>(
				std::forward<Pred>(pred));
	}

}// namespace dice::template_library

#endif// DICE_TEMPLATELIBRARY_HPP
