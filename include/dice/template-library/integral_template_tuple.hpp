#ifndef HYPERTRIE_INTEGRALTEMPLATEDTUPLE_HPP
#define HYPERTRIE_INTEGRALTEMPLATEDTUPLE_HPP

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <tuple>
#include <utility>

namespace dice::template_library {

	struct uniform_construct_t {};
	inline constexpr uniform_construct_t uniform_construct;

	struct individual_construct_t {};
	inline constexpr individual_construct_t individual_construct;

	namespace itt_detail {
		/**
		 * generates the std::integer_sequence<Int, first, ..., last>
		 * @note first is allowed to be greater than last
		 *
		 * @tparam Int the integral type of the std::integer_sequence
		 * @tparam first the starting integer
		 * @tparam last the end integer
		 */
		template<std::integral Int, Int first, Int last, Int ...ixs>
		constexpr auto make_integer_sequence(std::integer_sequence<Int, ixs...> = {}) {
			std::integer_sequence<Int, ixs..., first> const acc;

			if constexpr (first == last) {
				return acc;
			} else if constexpr (first < last) {
				return make_integer_sequence<Int, first + 1, last>(acc);
			} else {
				return make_integer_sequence<Int, first - 1, last>(acc);
			}
		}

		/**
		 * Selects the nth type from the given parameter pack
		 */
		template<size_t index, typename ...Ts>
		struct nth_type;

		template<typename First, typename ...Rest>
		struct nth_type<0, First, Rest...> {
			using type = First;
		};

		template<size_t index, typename First, typename ...Rest>
		struct nth_type<index, First, Rest...> {
			using type = typename nth_type<index - 1, Rest...>::type;
		};

		template<size_t, typename T>
		struct struct_tuple_leaf {
			T value_;

			constexpr struct_tuple_leaf() noexcept(std::is_nothrow_default_constructible_v<T>)
				: value_{} {
			}

			template<typename Arg>
			explicit constexpr struct_tuple_leaf(individual_construct_t, Arg &&arg) noexcept(std::is_nothrow_constructible_v<T, decltype(std::forward<Arg>(arg))>)
				: value_{std::forward<Arg>(arg)} {
			}

			constexpr auto operator<=>(struct_tuple_leaf const &other) const noexcept = default;
		};

		template<typename Markers, typename ...Ts>
		struct struct_tuple_base;

		template<size_t ...ixs, typename ...Ts>
		struct struct_tuple_base<std::index_sequence<ixs...>, Ts...> : struct_tuple_leaf<ixs, Ts>... {
			constexpr struct_tuple_base() noexcept((std::is_default_constructible_v<Ts> && ...)) = default;

			template<typename ...Args> requires (sizeof...(Args) == sizeof...(Ts))
			explicit constexpr struct_tuple_base(Args &&...args) noexcept((std::is_nothrow_constructible_v<Ts, decltype(std::forward<Args>(args))> && ...))
				: struct_tuple_leaf<ixs, Ts>{individual_construct, std::forward<Args>(args)}... {
			}

			template<size_t ix>
			[[nodiscard]] constexpr auto const &get() const & noexcept {
				return static_cast<typename nth_type<ix, struct_tuple_leaf<ixs, Ts>...>::type const &>(*this).value_;
			}

			template<size_t ix>
			[[nodiscard]] constexpr auto &get() & noexcept {
				return static_cast<typename nth_type<ix, struct_tuple_leaf<ixs, Ts>...>::type &>(*this).value_;
			}

			template<size_t ix>
			[[nodiscard]] constexpr auto const &&get() const && noexcept {
				return static_cast<typename nth_type<ix, struct_tuple_leaf<ixs, Ts>...>::type const &&>(*this).value_;
			}

			template<size_t ix>
			[[nodiscard]] constexpr auto &&get() && noexcept {
				return static_cast<typename nth_type<ix, struct_tuple_leaf<ixs, Ts>...>::type &&>(*this).value_;
			}

			template<typename Self, typename Visitor>
			[[nodiscard]] static constexpr decltype(auto) visit(Self &&self, Visitor &&visitor) {
				return (std::invoke(visitor, std::forward<Self>(self).template get<ixs>()), ...);
			}

			constexpr auto operator<=>(struct_tuple_base const &other) const noexcept = default;
		};

		/**
		 * A std::tuple-like type that is guaranteed to have struct-equivalent layout.
		 * I.e. layout(struct_tuple<Ts...>) == layout(struct { Ts... })
		 *
		 * This type exists because the standard does not have any layout guarantees for std::tuple,
		 * so its members can be in arbitrary order (and offset).
		 */
		template<typename ...Ts>
		struct struct_tuple : struct_tuple_base<std::make_index_sequence<sizeof...(Ts)>, Ts...> {
			static constexpr size_t size = sizeof...(Ts);

			constexpr struct_tuple() noexcept((std::is_nothrow_default_constructible_v<Ts> && ...)) = default;

			template<typename ...Args> requires (sizeof...(Args) == sizeof...(Ts))
			explicit constexpr struct_tuple(Args &&...args) noexcept((std::is_nothrow_constructible_v<Ts, decltype(std::forward<Args>(args))> && ...))
				: struct_tuple_base<std::make_index_sequence<sizeof...(Ts)>, Ts...>{std::forward<Args>(args)...} {
			}

			constexpr auto operator<=>(struct_tuple const &other) const noexcept = default;
		};

		/**
		 * Generates the actual tuple type
		 * for an integral_template_tuple<first, last, T> where Ixs = 0, 1, ..., last - first
		 * aka struct_tuple<T<first + 0>, T<first + 1>, ...>.
		 *
		 * Note: This function does not have an implementation because it is only used in decltype context.
		 */
		template<std::integral Int, template<Int> typename T, Int ...ixs>
		struct_tuple<T<ixs>...> make_itt_type_impl(std::integer_sequence<Int, ixs...>);

		/**
		 * Generates the std::tuple type corresponding to
		 * integral_template_variant<first, last, T> by calling make_itv_type_impl with the correct index_sequence.
		 *
		 * Note: only callable in decltype context
		 */
		template<std::integral Int, Int first, Int last, template<Int> typename T>
		auto make_itt_type() {
			return make_itt_type_impl<Int, T>(make_integer_sequence<Int, first, last>());
		}

		template<std::integral Int, Int first, Int last, template<Int> typename T>
		using itt_type_t = std::invoke_result_t<decltype(make_itt_type<Int, first, last, T>)>;
	} // namespace itt_detail

	/**
	 * A std::tuple-like type holding elements T<first> .. T<last> (inclusive).
	 *
	 * @tparam first first ix to provide to T<ix>
	 * @tparam last last ix to provide to T<ix>
	 * @tparam T the template that gets instantiated with T<ix> for ix in first..last (inclusive)
	 *
	 * @note first is allowed to be smaller then last.
	 */
	template<std::integral auto first, decltype(first) last, template<decltype(first)> typename T>
	struct integral_template_tuple {
		using index_type = decltype(first);

		template<index_type ix>
		using value_type = T<ix>;

	private:
		using underlying_type = itt_detail::itt_type_t<index_type, first, last, T>;
		underlying_type repr_;

		template<index_type ix>
		static consteval void check_ix() {
			static_assert(ix >= std::min(first, last) && ix <= std::max(first, last), "Index out of range");
		}

		template<index_type ix>
		static consteval size_t make_index() {
			return first <= last ? static_cast<size_t>(ix - first) : static_cast<size_t>(first - ix);
		}

		template<typename ...Args, index_type ...ixs>
		static constexpr underlying_type make_underlying(std::integer_sequence<index_type, ixs...>, Args &&...args) noexcept((std::is_nothrow_constructible_v<value_type<ixs>, decltype(args)...> && ...)) {
			return underlying_type{value_type<ixs>{args...}...};
		}

	public:
		constexpr integral_template_tuple() noexcept(std::is_nothrow_default_constructible_v<underlying_type>) = default;
		constexpr integral_template_tuple(integral_template_tuple const &other) noexcept(std::is_nothrow_copy_constructible_v<underlying_type>) = default;
		constexpr integral_template_tuple(integral_template_tuple &&other) noexcept(std::is_nothrow_move_constructible_v<underlying_type>) = default;
		constexpr integral_template_tuple &operator=(integral_template_tuple const &other) noexcept(std::is_nothrow_copy_assignable_v<underlying_type>) = default;
		constexpr integral_template_tuple &operator=(integral_template_tuple &&other) noexcept(std::is_nothrow_move_assignable_v<underlying_type>) = default;
		constexpr ~integral_template_tuple() noexcept(std::is_nothrow_destructible_v<underlying_type>) = default;

		/**
		 * Constructs the integral_template_tuple by providing each element one of args...
		 * @param args args to provide to the elements
		 */
		template<typename ...Args>
		explicit constexpr integral_template_tuple(individual_construct_t, Args &&...args) noexcept(std::is_nothrow_constructible_v<underlying_type, decltype(std::forward<Args>(args))...>)
			: repr_{std::forward<Args>(args)...} {
		}

		/**
		 * Uniformly constructs the integral_template_tuple by providing each element with args...
		 * @param args args to provide to each element for construction
		 */
		template<typename ...Args>
		explicit constexpr integral_template_tuple(uniform_construct_t, Args &&...args) noexcept(noexcept(make_underlying(itt_detail::make_integer_sequence<index_type, first, last>(), std::forward<Args>(args)...)))
			: repr_{make_underlying(itt_detail::make_integer_sequence<index_type, first, last>(), std::forward<Args>(args)...)} {
		}

		[[nodiscard]] static constexpr size_t size() noexcept {
			return underlying_type::size;
		}

		template<index_type ix>
		[[nodiscard]] constexpr value_type<ix> const &get() const & noexcept {
			check_ix<ix>();
			return repr_.template get<make_index<ix>()>();
		}

		template<index_type ix>
		[[nodiscard]] constexpr value_type<ix> &get() & noexcept {
			check_ix<ix>();
			return repr_.template get<make_index<ix>()>();
		}

		template<index_type ix>
		[[nodiscard]] constexpr value_type<ix> const &get() const && noexcept {
			check_ix<ix>();
			return std::move(repr_).template get<make_index<ix>()>();
		}

		template<index_type ix>
		[[nodiscard]] constexpr value_type<ix> &get() && noexcept {
			check_ix<ix>();
			return std::move(repr_).template get<make_index<ix>()>();
		}

		/**
		 * Visits each element using visitor
		 *
		 * @param visitor function to be called on each element
		 * @return whatever the last invocation (on the last element) of visitor returned
		 */
		template<typename Visitor>
		constexpr decltype(auto) visit(Visitor &&visitor) const & {
			return underlying_type::visit(repr_, std::forward<Visitor>(visitor));
		}

		template<typename Visitor>
		constexpr decltype(auto) visit(Visitor &&visitor) & {
			return underlying_type::visit(repr_, std::forward<Visitor>(visitor));
		}

		template<typename Visitor>
		constexpr decltype(auto) visit(Visitor &&visitor) const && {
			return underlying_type::visit(std::move(repr_), std::forward<Visitor>(visitor));
		}

		template<typename Visitor>
		constexpr decltype(auto) visit(Visitor &&visitor) && {
			return underlying_type::visit(std::move(repr_), std::forward<Visitor>(visitor));
		}

		constexpr auto operator<=>(integral_template_tuple const &other) const noexcept = default;

		/**
		 * Returns a reference to the subtuple of this obtained by dropping every element T<IX> where IX not in first..new_last (inclusive)
		 *
		 * @tparam new_first new first value
		 * @tparam new_last new last value
		 * @return subtuple
		 */
		template<index_type new_first, index_type new_last>
		constexpr integral_template_tuple<new_first, new_last, T> const &subtuple() const noexcept {
			static_assert(first <= last ? (new_first >= first && new_last <= last) : (new_first <= first && new_last >= last),
						  "Cannot add elements to a tuple by casting");

			return *reinterpret_cast<integral_template_tuple<new_first, new_last, T> const *>(this);
		}

		/**
		 * Returns a reference to the subtuple of this obtained by dropping every element T<IX> where IX not in first..new_last (inclusive)
		 *
		 * @tparam new_first new first value
		 * @tparam new_last new last value
		 * @return subtuple
		 */
		template<index_type new_first, index_type new_last>
		constexpr integral_template_tuple<new_first, new_last, T> &subtuple() noexcept {
			static_assert(first <= last ? (new_first >= first && new_last <= last) : (new_first <= first && new_last >= last),
						  "Cannot add elements to a tuple by casting");

			return *reinterpret_cast<integral_template_tuple<new_first, new_last, T> *>(this);
		}
	};
}// namespace dice::template_library

#endif//HYPERTRIE_INTEGRALTEMPLATEDTUPLE_HPP
