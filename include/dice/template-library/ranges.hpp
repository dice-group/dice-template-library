#ifndef DICE_TEMPLATELIBRARY_HPP
#define DICE_TEMPLATELIBRARY_HPP

#include <dice/template-library/next_to_range.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <set>
#include <unordered_set>

// all_of, any_of, none_of terminal algorithm
#define DTL_DEFINE_PIPEABLE_RANGE_ALGO(algo_name)                                      \
	namespace ranges_algo_detail {                                                     \
		template<typename Pred>                                                        \
		struct algo_name##_fn {                                                        \
		private:                                                                       \
			Pred pred_;                                                                \
                                                                                       \
		public:                                                                        \
			explicit constexpr algo_name##_fn(Pred const &pred) : pred_{pred} {}       \
			explicit constexpr algo_name##_fn(Pred &&pred) : pred_{std::move(pred)} {} \
                                                                                       \
			/* input_range causes no dangling risk since this adaptor is terminal  */  \
			template<std::ranges::input_range R>                                       \
			constexpr bool operator()(R &&r) const {                                   \
				return std::ranges::algo_name(r, pred_);                               \
			}                                                                          \
                                                                                       \
			template<typename R>                                                       \
			friend auto operator|(R &&r, algo_name##_fn const &self) {                 \
				return self(std::forward<R>(r));                                       \
			}                                                                          \
		};                                                                             \
	}                                                                                  \
                                                                                       \
	template<typename Pred>                                                            \
	constexpr auto algo_name(Pred &&pred) {                                            \
		return ranges_algo_detail::algo_name##_fn<std::remove_cvref_t<Pred>>(          \
				std::forward<Pred>(pred));                                             \
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
				static_assert(requires { std::ranges::empty(range); }, "std::ranges::empty requires an input range as it might otherwise be expensive to materialize the first element.");
				return std::ranges::empty(range);
			}

			template<std::ranges::input_range R>
			friend constexpr bool operator|(R &&range, empty_fn const &self) {
				return self(std::forward<R>(range));
			}
		};

		struct non_empty_fn {
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
		template<typename T, typename Pred>
		struct remove_element_fn {
		private:
			T value_;
			Pred pred_;

		public:
			template<typename V, typename P>
			constexpr remove_element_fn(V &&value, P &&pred)
				: value_{std::forward<V>(value)}, pred_{std::forward<P>(pred)} {
			}

			template<std::ranges::viewable_range R>// viewable_range ensures there are no views into dangling stuff
			constexpr auto operator()(R &&range) const {
                return std::forward<R>(range)
                       | std::views::filter([value = value_, pred = pred_](auto const &element) {
                             return !std::invoke(pred, element, value);
                         });
            }

			template<std::ranges::viewable_range R>// viewable_range ensures there are no views into dangling stuff
			friend constexpr auto operator|(R &&range, remove_element_fn const &self) {
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
		return ranges_algo_detail::remove_element_fn<std::remove_cvref_t<T>, std::remove_cvref_t<Pred>>{
				std::forward<T>(value), std::forward<Pred>(pred)};
	}
}// namespace dice::template_library

// all_equal adaptor
namespace dice::template_library {
	namespace ranges_algo_detail {
		template<typename Pred>
		struct all_equal_fn {
		private:
			Pred pred_;

		public:
			explicit constexpr all_equal_fn(Pred &&pred) : pred_{std::move(pred)} {}
			explicit constexpr all_equal_fn(Pred const &pred) : pred_{pred} {}

			template<std::ranges::input_range R>
				requires std::copyable<std::ranges::range_value_t<R>>
			constexpr bool operator()(R &&range) const {
				auto it = std::ranges::begin(range);
				auto const end = std::ranges::end(range);
				if (it == end) {
					return true;
				}

				auto const first_element = *it;
				++it;

                // Check if all subsequent elements in the range match the first one.
                return std::ranges::subrange(it, end)
                       | dice::template_library::all_of([this, first_element](auto const &current_element) {
                             return std::invoke(pred_, current_element, first_element);
                         });
			}

			template<std::ranges::input_range R>
				requires std::copyable<std::ranges::range_value_t<R>>
			friend constexpr bool operator|(R &&range, all_equal_fn const &self) {
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
		return ranges_algo_detail::all_equal_fn<std::remove_cvref_t<Pred>>(std::forward<Pred>(pred));
	}
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
	inline constexpr ranges_algo_detail::unique_fn unique{};

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
		template<typename Pred>
		struct all_distinct_fn {
		private:
			Pred pred_;

		public:
			explicit constexpr all_distinct_fn(Pred const &pred) : pred_{pred} {}
			explicit constexpr all_distinct_fn(Pred &&pred) : pred_{std::move(pred)} {}

			template<std::ranges::input_range R>
			constexpr bool operator()(R &&range) const {
				using value_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
				static_assert(unordered_set_elem<value_type> || (set_elem<value_type> && std::is_same_v<Pred, std::ranges::equal_to>),
							  "The elements of the consumed range are neither compatible with std::unordered_set nor with std::set.");

				auto it = std::ranges::begin(range);
				auto const end = std::ranges::end(range);
				if (it == end) {
					return true;
				}

				auto seen = [&] {
					if constexpr (unordered_set_elem<value_type>) {
						return std::unordered_set<value_type, std::hash<value_type>, Pred>{1, {}, pred_};
					} else {
						return std::set<value_type>{};
					}
				}();

				for (auto const &element : std::ranges::subrange(it, end)) {
					if (!seen.insert(element).second) {
						return false;
					}
				}
				return true;
			}

			template<std::ranges::input_range R>
			friend constexpr bool operator|(R &&range, all_distinct_fn const &self) {
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
		return ranges_algo_detail::all_distinct_fn<std::remove_cvref_t<Pred>>(
				std::forward<Pred>(pred));
	}


    /**
     *
     * @tparam R1
     * @tparam R2
     * @tparam Cmp
     * @tparam Proj1
     * @tparam Proj2
     */
    template<std::ranges::range R1, std::ranges::range R2, typename Cmp = std::ranges::less, typename Proj1 = std::identity, typename Proj2 = std::identity>
    requires (std::ranges::view<R1> && std::ranges::view<R2>)
    struct merge_view : std::ranges::view_interface<merge_view<R1, R2, Cmp, Proj1, Proj2>> {
    private:
        R1 r1_;
		R2 r2_;

		[[no_unique_address]] Cmp cmp_;
		[[no_unique_address]] Proj1 proj1_;
		[[no_unique_address]] Proj2 proj2_;

	    template<bool is_const>
		struct iterator_impl {
	    private:
	    	using base1 = std::conditional_t<is_const, R1 const, R1>;
	    	using base2 = std::conditional_t<is_const, R2 const, R2>;
	        using base_iterator1 = std::ranges::iterator_t<base1>;
	        using base_iterator2 = std::ranges::iterator_t<base2>;
	        using base_sentinel1 = std::ranges::sentinel_t<base1>;
	        using base_sentinel2 = std::ranges::sentinel_t<base2>;

	        enum struct Source : uint8_t {
	            Left,
                Right,
                Ended,
            };

	    public:
		    using value_type = std::common_type_t<
		        std::indirect_result_t<Proj1, base_iterator1>,
	            std::indirect_result_t<Proj2, base_iterator2>
	        >;

	    	using difference_type = std::common_type_t<
			    typename std::iterator_traits<base_iterator1>::difference_type,
			    typename std::iterator_traits<base_iterator2>::difference_type
		    >;

	    	using iterator_category = std::input_iterator_tag;

	        using iterator_concept = std::conditional_t<
                std::ranges::forward_range<base1> && std::ranges::forward_range<base2>,
                std::forward_iterator_tag,
                std::input_iterator_tag
            >;

		private:
		    std::conditional_t<is_const, merge_view const *, merge_view *> parent_ = nullptr;

		    base_iterator1 it1_{};
	        [[no_unique_address]] base_sentinel1 end1_{};
		    base_iterator2 it2_{};
	        [[no_unique_address]] base_iterator2 end2_{};

		    Source src_ = Source::Ended;

		    void update_state() {
		        bool const r1_ended = it1_ == end1_;
		        bool const r2_ended = it2_ == end2_;

		        if (r1_ended) {
		            if (r2_ended) {
		                src_ = Source::Ended;
		            } else {
		                src_ = Source::Right;
		            }
		        } else {
		            if (r2_ended) {
		                src_ = Source::Left;
		            } else {
		                src_ = std::invoke(parent_->cmp_, std::invoke(parent_->proj1_, *it1_), std::invoke(parent_->proj2_, *it2_)) ? Source::Left : Source::Right;
		            }
		        }
		    }

		public:
            iterator_impl()
                    requires (std::is_default_constructible_v<base_iterator1>
                              && std::is_default_constructible_v<base_iterator2>
                              && std::is_default_constructible_v<base_sentinel1>
                              && std::is_default_constructible_v<base_sentinel2>)
                    = default;

            explicit iterator_impl(std::conditional_t<is_const, merge_view const &, merge_view &> parent)
                : parent_{&parent},
                  it1_{std::ranges::begin(parent.r1_)},
                  end1_{std::ranges::end(parent.r1_)},
                  it2_{std::ranges::begin(parent.r2_)},
                  end2_{std::ranges::end(parent.r2_)} {
                update_state();
            }

		    value_type operator*() const {
		        switch (src_) {
		            case Source::Left: {
		                return std::invoke(parent_->proj1_, *it1_);
		            }
		            case Source::Right: {
		                return std::invoke(parent_->proj2_, *it2_);
		            }
		            default: {
		                assert(false);
		                __builtin_unreachable();
		            }
		        }
		    }

		    iterator_impl &operator++() {
		        switch (src_) {
		            case Source::Left: {
		                ++it1_;
		                break;
		            }
		            case Source::Right: {
		                ++it2_;
		                break;
		            }
		            default: {
		                assert(false);
		                __builtin_unreachable();
		            }
		        }

		        update_state();
		        return *this;
		    }

		    void operator++(int) requires (!std::is_copy_constructible_v<iterator_impl>) {
		        ++*this;
		    }

	    	iterator_impl operator++(int)
                    requires (std::is_copy_constructible_v<iterator_impl>)
            {
                auto cpy = *this;
                ++*this;
                return cpy;
            }

            friend bool operator==(iterator_impl const &lhs, iterator_impl const &rhs)
                    requires (std::equality_comparable<base_iterator1> && std::equality_comparable<base_iterator2>)
            {
                return lhs.it1_ == rhs.it1_ && lhs.it2_ == rhs.it2_;
            }

	        friend bool operator==(iterator_impl const &iter, std::default_sentinel_t) noexcept {
	    	    return iter.src_ == Source::Ended;
	    	}

	        friend bool operator==(std::default_sentinel_t, iterator_impl const &iter) noexcept {
	    	    return iter == std::default_sentinel;
	    	}
		};

	public:
	    using iterator = iterator_impl<false>;
	    using const_iterator = iterator_impl<true>;
	    using sentinel = std::default_sentinel_t;

		merge_view(R1 r1, R2 r2, Cmp cmp = {}, Proj1 proj1 = {}, Proj2 proj2 = {})
			: r1_{std::move(r1)},
			  r2_{std::move(r2)},
			  cmp_{std::move(cmp)},
			  proj1_{std::move(proj1)},
			  proj2_{std::move(proj2)} {
		}

	    [[nodiscard]] iterator begin() {
		    return iterator{*this};
		}

	    [[nodiscard]] const_iterator begin() const {
		    return const_iterator{*this};
		}

	    static constexpr sentinel end() noexcept {
		    return sentinel{};
		}

		[[nodiscard]] size_t size() const noexcept requires (std::ranges::sized_range<R1> && std::ranges::sized_range<R2>) {
			return std::ranges::size(r1_) + std::ranges::size(r2_);
		}
	};

    template<typename R1, typename R2, typename Cmp = std::ranges::less, typename Proj1 = std::identity, typename Proj2 = std::identity>
    merge_view(R1, R2, Cmp = {}, Proj1 = {}, Proj2 = {}) -> merge_view<std::views::all_t<R1>, std::views::all_t<R2>, Cmp, Proj1, Proj2>;

    /**
     * Merge two **sorted** ranges into a combined sorted range
     *
     * @param r1 first **sorted** range
     * @param r2 second **sorted** range
     * @param cmp comparator for projected values
     * @param proj1 projection for elements of first range
     * @param proj2 projection for elements of second range
     *
     * @return view over combined, sorted and projected range
     */
    template<std::ranges::range R1, std::ranges::range R2, typename Cmp = std::ranges::less, typename Proj1 = std::identity, typename Proj2 = std::identity>
    [[nodiscard]] std::ranges::view auto merge(R1 &&r1, R2 &&r2, Cmp &&cmp = {}, Proj1 &&proj1 = {}, Proj2 &&proj2 = {}) {
        return merge_view<std::views::all_t<R1>, std::views::all_t<R2>, std::remove_cvref_t<Cmp>, std::remove_cvref_t<Proj1>, std::remove_cvref_t<Proj2>>{
                std::forward<R1>(r1), std::forward<R2>(r2), std::forward<Cmp>(cmp), std::forward<Proj1>(proj1), std::forward<Proj2>(proj2)};
    }

}// namespace dice::template_library

#endif// DICE_TEMPLATELIBRARY_HPP
