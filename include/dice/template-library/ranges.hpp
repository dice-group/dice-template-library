#ifndef DICE_TEMPLATELIBRARY_HPP
#define DICE_TEMPLATELIBRARY_HPP

#include <dice/template-library/next_to_range.hpp>

#include <algorithm>
#include <functional>
#include <ranges>
#include <set>
#include <unordered_set>

// predicate based terminal algorithm
#define DTL_DEFINE_PREDICATE_BASE_RANGE_ALGO(algo_name, impl_function)                                     \
    namespace ranges_algo_detail {                                                                         \
        template<typename Pred>                                                                            \
        struct algo_name##_pipeline {                                                                      \
        private:                                                                                           \
            Pred pred_;                                                                                    \
                                                                                                           \
        public:                                                                                            \
            explicit constexpr algo_name##_pipeline(Pred pred)                                             \
                : pred_{std::move(pred)} {                                                                 \
            }                                                                                              \
                                                                                                           \
            template<std::ranges::input_range R>                                                           \
            [[nodiscard]] friend constexpr bool operator|(R &&r, algo_name##_pipeline const &self) {       \
                return impl_function(std::ranges::begin(r), std::ranges::end(r), self.pred_);              \
            }                                                                                              \
        };                                                                                                 \
                                                                                                           \
        struct algo_name##_fn {                                                                            \
            template<std::input_iterator I, std::sentinel_for<I> S, typename Pred = std::ranges::equal_to> \
            [[nodiscard]] constexpr bool operator()(I first, S last, Pred pred = {}) const {               \
                return impl_function(std::move(first), std::move(last), std::move(pred));                  \
            }                                                                                              \
                                                                                                           \
            template<std::ranges::input_range R, typename Pred = std::ranges::equal_to>                    \
            [[nodiscard]] constexpr bool operator()(R &&range, Pred pred = {}) const {                     \
                return impl_function(std::ranges::begin(range), std::ranges::end(range), std::move(pred)); \
            }                                                                                              \
                                                                                                           \
            template<typename Pred = std::ranges::equal_to>                                                \
            requires (!std::ranges::input_range<Pred> && !std::input_iterator<Pred>)                       \
            [[nodiscard]] constexpr auto operator()(Pred pred = {}) const {                                \
                return algo_name##_pipeline{std::move(pred)};                                              \
            }                                                                                              \
        };                                                                                                 \
    }                                                                                                      \
                                                                                                           \
    inline constexpr ranges_algo_detail::algo_name##_fn algo_name;

namespace dice::template_library {
	DTL_DEFINE_PREDICATE_BASE_RANGE_ALGO(all_of, std::ranges::all_of);
	DTL_DEFINE_PREDICATE_BASE_RANGE_ALGO(any_of, std::ranges::any_of);
	DTL_DEFINE_PREDICATE_BASE_RANGE_ALGO(none_of, std::ranges::none_of);
}// namespace dice::template_library

// empty / non_empty terminal algorithm
namespace dice::template_library {
	namespace ranges_algo_detail {
		struct empty_fn {
			template<std::input_iterator I, std::sentinel_for<I> S>
			constexpr bool operator()(I first, S last) const {
				std::ranges::subrange<I, S> range{std::move(first), std::move(last)};
				static_assert(requires { std::ranges::empty(range); }, "std::ranges::empty requires a forward range as it might otherwise be expensive to materialize the first element.");
				return std::ranges::empty(range);
			}

			template<std::ranges::input_range R>
			constexpr bool operator()(R &&range) const {
				static_assert(requires { std::ranges::empty(range); }, "std::ranges::empty requires a forward range as it might otherwise be expensive to materialize the first element.");
				return std::ranges::empty(std::forward<R>(range));
			}

			template<std::ranges::input_range R>
			friend constexpr bool operator|(R &&range, empty_fn const &self) {
				return self(std::forward<R>(range));
			}
		};

		struct non_empty_fn {
			template<std::input_iterator I, std::sentinel_for<I> S>
			constexpr bool operator()(I first, S last) const {
				return !empty_fn{}(std::move(first), std::move(last));
			}

			template<std::ranges::input_range R>
			constexpr bool operator()(R &&range) const {
				return !empty_fn{}(std::forward<R>(range));
			}

			template<std::ranges::input_range R>
			friend constexpr bool operator|(R &&range, non_empty_fn const &self) {
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
		template<std::ranges::viewable_range R, typename T, typename Pred = std::ranges::equal_to>
		constexpr std::ranges::view auto remove_element_impl(R &&range, T remove_value, Pred pred = {}) {
            return std::forward<R>(range)
                   | std::views::filter([remove_value = std::move(remove_value), pred = std::move(pred)](auto const &element) {
                         return !std::invoke(pred, element, remove_value);
                     });
		}

		template<typename T, typename Pred>
		struct remove_element_pipeline {
		private:
			T value_;
			Pred pred_;

		public:
			template<typename V, typename P>
			constexpr remove_element_pipeline(V &&value, P &&pred)
				: value_{std::forward<V>(value)}, pred_{std::forward<P>(pred)} {
			}

			template<std::ranges::viewable_range R>
			friend constexpr std::ranges::view auto operator|(R &&range, remove_element_pipeline const &self) {
				return remove_element_impl(std::forward<R>(range), self.value_, self.pred_);
			}
		};

		struct remove_element_fn {
			template<typename T, typename Pred = std::ranges::equal_to>
			constexpr auto operator()(T remove_value, Pred pred = {}) const {
				return remove_element_pipeline<T, Pred>{std::move(remove_value), std::move(pred)};
			}
		};
	}// namespace ranges_algo_detail

	/**
	 * Creates a lazy, pipeable view of a range that excludes elements matching a given value. This adaptor does not modify the source range.
	 *
	 * @param value The value to filter out of the source range.
	 * @param pred Optional binary predicate for comparison. Defaults to `std::ranges::equal_to`.
	 * @return A range adaptor closure.
	 *
	 * @note This view is deprecated because it is not possible to make it callable as a regular function and pipelineable
	 *		at the same time due to overload resolution issues with ranges of ranges.
	 *		Additionally, this is just a `std::views::filter([remove_value](auto const &x) { return x != remove_value });`, use that instead.
	 */
	[[deprecated("Use std::views::filter")]] inline constexpr ranges_algo_detail::remove_element_fn remove_element;

}// namespace dice::template_library

// all_equal adaptor
namespace dice::template_library {
	namespace ranges_algo_detail {
        template<std::input_iterator I, std::sentinel_for<I> S, typename Pred = std::ranges::equal_to>
        requires (std::copyable<std::iter_value_t<I>>)
        bool all_equal_impl(I first, S last, Pred pred) {
            if (first == last) {
                return true;
            }

            auto const first_element = *first;
            ++first;

            // Check if all subsequent elements in the range match the first one.
            return std::ranges::subrange(first, last)
                   | dice::template_library::all_of([&](auto const &current_element) {
                         return std::invoke(pred, current_element, first_element);
                     });
        }
	}// namespace ranges_algo_detail

	/**
     * @brief Checks if all elements in a range are equal to each other.
     *
     * This is a pipeable, terminal algorithm that returns a boolean. Elements must be copyable for this to work.
     *
     * @param pred Optional binary predicate for comparison. Defaults to `std::ranges::equal_to`.
     * @return true if all elements in the range are equal, otherwise false.
     */
	DTL_DEFINE_PREDICATE_BASE_RANGE_ALGO(all_equal, ranges_algo_detail::all_equal_impl);
}// namespace dice::template_library

// unique adaptor
namespace dice::template_library {
	namespace ranges_algo_detail {

		template<typename T>
        concept unordered_set_elem = std::equality_comparable<T> && requires (T const &elem) {
            { std::hash<T>{}(elem) } -> std::convertible_to<size_t>;
        };

		template<typename T>
		concept set_elem = std::strict_weak_order<std::less<T>, T, T>;

		template<std::ranges::input_range R>
			requires std::ranges::view<R>
		struct unique_view : std::ranges::view_interface<unique_view<R>> {
			struct iterator;
			using sentinel = std::default_sentinel_t;

		private:
			R base_;

		public:
			explicit constexpr unique_view(R const &base) : base_{base} {}
			explicit constexpr unique_view(R &&base) : base_{std::move(base)} {}

			constexpr iterator begin() {
				return iterator{*this};
			}

			constexpr sentinel end() const noexcept {
				return std::default_sentinel;
			}
		};

		template<std::ranges::input_range R>
			requires std::ranges::view<R>
		struct unique_view<R>::iterator {
			using iterator_category = std::input_iterator_tag;
			using value_type = std::ranges::range_value_t<R>;
			using difference_type = std::ranges::range_difference_t<R>;

		private:
			using set_value_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;

			static_assert(std::copy_constructible<set_value_type>,
						  "The value type for unique must be copy-constructible.");

			static_assert(set_elem<set_value_type> || unordered_set_elem<set_value_type>,
						  "The value type must be either hashable and equality comparable, or support less than.");

			using set_type = std::conditional_t<
					unordered_set_elem<set_value_type>,// prioritize unordered_set
					std::unordered_set<set_value_type>,
					std::set<set_value_type>>;

			set_type seen_;
			std::ranges::iterator_t<R> current_;
			[[no_unique_address]] std::ranges::sentinel_t<R> end_;

			void find_next_unique() {
				while (current_ != end_) {
					if (seen_.insert(*current_).second) {
						break;
					}
					++current_;
				}
			}

		public:
			explicit constexpr iterator(unique_view &parent)
				: seen_{},
				  current_{std::ranges::begin(parent.base_)},
				  end_{std::ranges::end(parent.base_)} {
				find_next_unique();
			}

			constexpr decltype(auto) operator*() const {
				return *current_;
			}

			constexpr iterator &operator++() {
				++current_;
				find_next_unique();
				return *this;
			}

			constexpr void operator++(int) {
				++*this;
			}

			friend constexpr bool operator==(iterator const &self, std::default_sentinel_t) {
				return self.current_ == self.end_;
			}

			friend constexpr bool operator==(std::default_sentinel_t sent, iterator const &self) {
				return self == sent;
			}
		};

		struct unique_fn {
			template<std::ranges::viewable_range R>
			constexpr auto operator()(R &&r) const {
				return unique_view(std::views::all(std::forward<R>(r)));
			}

			template<std::ranges::viewable_range R>
			friend constexpr auto operator|(R &&r, unique_fn const &self) {
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
	inline constexpr ranges_algo_detail::unique_fn unique;

}// namespace dice::template_library


namespace dice::template_library {

    template<typename S, typename T>
    concept step_for = std::is_default_constructible_v<S> && requires (T start, T const stop, S step) {
        { start <= stop } -> std::convertible_to<bool>;
        { start >= stop } -> std::convertible_to<bool>;

        start += step;
    };

	namespace ranges_algo_detail {
		/**
		 * range generator (python-like iota with step) implemented as a rust-like next() iterator
		 *
		 * @tparam T range return type
		 * @tparam S step type for range (can also be a signed type even though T is unsigned)
		 */
		template<typename T, step_for<T> S>
		struct range_iterator {
			using value_type = T;

		private:
			T current_;
			T stop_;
			S step_;

		public:
            range_iterator() noexcept
                    requires (std::is_default_constructible_v<T> && std::is_constructible_v<S, int>)
                : current_{},
                  stop_{},
                  step_{1} {
            }

			explicit constexpr range_iterator(T start, T stop, S step)
				: current_{start}, stop_{stop}, step_{step} {
				if (step == S{}) [[unlikely]] {
					throw std::invalid_argument{"range: step must not be the zero element/the additive identity"};
				}
			}

			[[nodiscard]] constexpr std::optional<T> next() noexcept {
				if (step_ > S{}) {
					if (current_ >= stop_) {
						return std::nullopt;
					}
				} else {
					if (current_ <= stop_) {
						return std::nullopt;
					}
				}

				return std::exchange(current_, current_ + step_);
			}

            [[nodiscard]] constexpr size_t remaining() const noexcept
                    requires (std::integral<T> && std::integral<S>)
            {
				if (current_ <= stop_) {
					// forward
					if (step_ < S{}) {
						// wrong direction
						return 0;
					}

					return static_cast<size_t>((stop_ - current_ + step_ - 1) / step_);
				} else {
					// backward
					if (step_ > S{}) {
						// wrong direction
						return 0;
					}

					return static_cast<size_t>((current_ - stop_ + -step_ - 1) / -step_);
				}
			}

            [[nodiscard]] constexpr std::optional<T> nth(size_t off) noexcept
                    requires (std::integral<T> && std::integral<S>)
            {
				auto const rem = remaining();
				if (off >= rem) {
					current_ = stop_;
					return std::nullopt;
				}

				current_ += static_cast<T>(off) * step_;
				return next();
			}

            [[nodiscard]] constexpr std::optional<T> next_back() noexcept
                    requires (std::integral<T> && std::integral<S>)
            {
				if (step_ > S{}) {
					if (current_ >= stop_) {
						return std::nullopt;
					}
					stop_ -= step_;
					return stop_;
				} else {
					if (current_ <= stop_) {
						return std::nullopt;
					}
					stop_ -= step_;
					return stop_;
				}
			}

            [[nodiscard]] constexpr std::optional<T> nth_back(size_t off) noexcept
                    requires (std::integral<T> && std::integral<S>)
			{
				auto const rem = remaining();
				if (off >= rem) {
					stop_ = current_;
					return std::nullopt;
				}

				stop_ -= static_cast<T>(off) * step_;
				return next_back();
			}
		};
	}// namespace ranges_algo_detail

	template<typename T, typename S>
	using range_view = next_to_view<ranges_algo_detail::range_iterator<T, S>>;

	/**
	 * @brief A lazy view that generates a sequence of numbers, similar to Python's range().
	 *
	 * This view can be used with one, two, or three arguments:
	 * - `range<T>(stop)`: Generates [0,stop).
	 * - `range<T>(start, stop)`: Generates [start,stop).
	 * - `range<T>(start, stop, step)`: Generates numbers from start, incrementing by step, until stop is met or passed.
	 */
	template<typename T, typename S>
	[[nodiscard]] constexpr range_view<T, S> range(T start, T stop, S step) {
		return range_view<T, S>{start, stop, step};
	}

	template<typename T>
    requires (std::is_constructible_v<T, int> && step_for<T, T>)
    [[nodiscard]] constexpr range_view<T, T> range(T start, T stop) {
        return range_view<T, T>{start, stop, T(1)};
    }

    template<typename T>
    requires (std::is_default_constructible_v<T> && std::is_constructible_v<T, int>)
    [[nodiscard]] constexpr range_view<T, T> range(T stop) {
		return range_view<T, T>{T{}, stop, T(1)};
	}

}// namespace dice::template_library

// all_distinct terminal algorithm
namespace dice::template_library {
	namespace ranges_algo_detail {
		template<std::input_iterator I, std::sentinel_for<I> S, typename Pred = std::ranges::equal_to>
		[[nodiscard]] constexpr bool all_distinct_impl(I first, S last, Pred pred) {
			using value_type = std::remove_cvref_t<std::iter_value_t<I>>;
			static_assert(unordered_set_elem<value_type> || (set_elem<value_type> && std::is_same_v<Pred, std::ranges::equal_to>),
						  "The elements of the consumed range are neither compatible with std::unordered_set nor with std::set.");

			if (first == last) {
				return true;
			}

			auto seen = [&] {
				if constexpr (unordered_set_elem<value_type>) {
					return std::unordered_set<value_type, std::hash<value_type>, Pred>{1, {}, std::move(pred)};
				} else {
					return std::set<value_type>{};
				}
			}();

			for (; first != last; ++first) {
				if (!seen.insert(*first).second) {
					return false;
				}
			}
			return true;
		}

		template<std::input_iterator I, std::sentinel_for<I> S, typename Pred = std::ranges::equal_to>
		[[nodiscard]] constexpr bool all_distinct_sorted_impl(I first, S last, Pred pred) {
			return std::ranges::adjacent_find(first, last, std::move(pred)) == last;
		}
	} // namespace ranges_algo_detail

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
	DTL_DEFINE_PREDICATE_BASE_RANGE_ALGO(all_distinct, ranges_algo_detail::all_distinct_impl);
} // namespace dice::template_library

// is_sorted_unique terminal algorithm
namespace dice::template_library {
	namespace ranges_algo_detail {
        template<std::input_iterator I, std::sentinel_for<I> S, typename Cmp, typename Proj>
        requires (std::indirect_strict_weak_order<Cmp, std::projected<I, Proj>>)
        constexpr bool is_sorted_unique_impl(I first, S last, Cmp cmp, Proj proj) {
            if (first == last) {
                return true;
            }

            auto it = first;
            while (++it != last) {
                // We require that 'prev' is strictly less than 'curr'.
                // cmp(a, b) ~ a < b
                // <-> !cmp(a, b) ~ a >= b
                if (!std::invoke(cmp, std::invoke(proj, *first), std::invoke(proj, *it))) {
                    return false;
                }
                first = it;
            }
            return true;
        }

		template<typename Cmp, typename Proj>
		struct is_sorted_unique_pipeline {
        private:
        	[[no_unique_address]] Cmp cmp_;
        	[[no_unique_address]] Proj proj_;

        public:
        	is_sorted_unique_pipeline(Cmp cmp, Proj proj)
			: cmp_{std::move(cmp)}, proj_{std::move(proj)} {
        	}

        	template<std::ranges::input_range R>
			[[nodiscard]] friend constexpr bool operator|(R &&range, is_sorted_unique_pipeline const &self) {
        		return is_sorted_unique_impl(std::ranges::begin(range), std::ranges::end(range), self.cmp_, self.proj_);
        	}
        };

		struct is_sorted_unique_fn {
            /**
			 * Return true iff the given range is sorted and there are no duplicates.
			 *
			 * @param range range to check
			 * @param cmp comparator
			 * @param proj projection
			 */
            template<std::ranges::input_range R, typename Cmp = std::ranges::less, typename Proj = std::identity>
            [[nodiscard]] constexpr bool operator()(R &&range, Cmp cmp = {}, Proj proj = {}) const {
                return is_sorted_unique_impl(std::ranges::begin(range), std::ranges::end(range), std::move(cmp), std::move(proj));
            }

            /**
		     * Return true iff the given range is sorted and there are no duplicates.
		     *
		     * @param first iterator to start of range
		     * @param last sentinel for range
		     * @param cmp comparator
		     * @param proj projection
		     * @return
		     */
            template<std::input_iterator I, std::sentinel_for<I> S, typename Cmp = std::ranges::less, typename Proj = std::identity>
            [[nodiscard]] constexpr bool operator()(I first, S last, Cmp cmp = {}, Proj proj = {}) const {
                return is_sorted_unique_impl(std::move(first), std::move(last), std::move(cmp), std::move(proj));
            }

            /**
			 * Pipeline overload for is_strictly_sorted
			 */
            template<typename Cmp = std::ranges::less, typename Proj = std::identity>
            requires (!std::ranges::input_range<Cmp> && !std::input_iterator<Cmp>)
            [[nodiscard]] constexpr auto operator()(Cmp cmp = {}, Proj proj = {}) const {
                return is_sorted_unique_pipeline{std::move(cmp), std::move(proj)};
            }
		};
	} // namespace ranges_algo_detail

	/**
     * Return true iff the given range is sorted and there are no duplicates.
     *
     * @param range or first+last range to check
     * @param cmp comparator
     * @param proj projection
     */
	inline constexpr ranges_algo_detail::is_sorted_unique_fn is_sorted_unique;
}// namespace dice::template_library

#endif// DICE_TEMPLATELIBRARY_HPP
