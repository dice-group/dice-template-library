#ifndef DICE_TEMPLATELIBRARY_TYPELIST_HPP
#define DICE_TEMPLATELIBRARY_TYPELIST_HPP

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace dice::template_library::type_list {

	/**
     * A list of types.
     * @tparam Ts types
     */
    template<typename ...Ts>
    struct type_list {
    };


	/**
     * Find the size of a type list.
     *
     * @tparam TL type list
     */
    template<typename TL>
    struct size;

    template<typename ...Ts>
    struct size<type_list<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {
    };

    template<typename TL>
    inline constexpr size_t size_v = size<TL>::value;


	/**
     * Find the first element of a type list.
     * If the type list is empty there will be no member typedef; use `opt` if this is undesired.
     *
     * @tparam TL type list
     */
    template<typename TL>
    struct first;

    template<>
    struct first<type_list<>> {
    };

    template<typename T, typename ...Ts>
    struct first<type_list<T, Ts...>> {
        using type = T;
    };

    template<typename TL>
    using first_t = typename first<TL>::type;


	/**
     * Find the last element of a type list.
     * If the type list is empty there will be no member typedef; use `opt` if this is undesired.
     *
     * @tparam TL type list
     */
    template<typename TL>
    struct last {
    };

    template<>
    struct last<type_list<>> {
    };

    template<typename T, typename ...Ts>
    struct last<type_list<T, Ts...>> : last<type_list<Ts...>> {
    };

    template<typename T>
    struct last<type_list<T>> {
        using type = T;
    };

    template<typename TL>
    using last_t = typename last<TL>::type;


	/**
     * Find the nth element of a type list.
     * If the given index is not a valid index in the type list, or the type list is empty there will be no member typedef; use `opt` if this is undesired.
     *
     * @tparam TL type list
     * @tparam idx index of the element
     */
    template<typename TL, size_t idx>
    struct nth;

    template<size_t idx>
    struct nth<type_list<>, idx> {
    };

    template<typename T, typename ...Ts>
    struct nth<type_list<T, Ts...>, 0> {
        using type = T;
    };

    template<typename T, typename ...Ts, size_t idx>
    struct nth<type_list<T, Ts...>, idx> : nth<type_list<Ts...>, idx - 1> {
    };

    template<typename TL, size_t idx>
    using nth_t = typename nth<TL, idx>::type;


	/**
     * Concatenate any number of type lists.
     *
     * @tparam Lists zero or more type lists to concatenate
     */
    template<typename... Lists>
    struct concat;

    template<>
    struct concat<> {
        using type = type_list<>;
    };

    template<typename List>
    struct concat<List> {
        using type = List;
    };

    template<typename ...Ts, typename ...Us>
    struct concat<type_list<Ts...>, type_list<Us...>> {
        using type = type_list<Ts..., Us...>;
    };

    template<typename Head, typename... Tail>
    struct concat<Head, Tail...> {
        using type = typename concat<
            Head,
            typename concat<Tail...>::type
        >::type;
    };

    template<typename... Lists>
    using concat_t = typename concat<Lists...>::type;


	/**
     * Remove a specified number of elements from the start of the type list
     *
     * @tparam TL type list
     * @tparam count number of types to drop
     */
    template<typename TL, size_t count>
    struct drop;

    template<typename ...Ts>
    struct drop<type_list<Ts...>, 0> {
        using type = type_list<Ts...>;
    };

    template<typename T, typename ...Ts, size_t count> requires (count > 0)
    struct drop<type_list<T, Ts...>, count> {
        using type = typename drop<type_list<Ts...>, count - 1>::type;
    };

    template<typename TL, size_t count>
    using drop_t = typename drop<TL, count>::type;


	/**
     * Keep only the first count elements from the type list.
     *
     * @tparam TL type list
     * @tparam count number of elements to keep
     */
    template<typename TL, size_t count>
    struct take;

    template<typename ...Ts>
    struct take<type_list<Ts...>, 0> {
        using type = type_list<>;
    };

    template<typename T, typename ...Ts, size_t count> requires (count > 0)
    struct take<type_list<T, Ts...>, count> {
        using type = concat_t<type_list<T>, typename take<type_list<Ts...>, count - 1>::type>;
    };

    template<typename TL, size_t count>
    using take_t = typename take<TL, count>::type;


	/**
     * Apply the types in a type list to a template type.
     *
     * @tparam TL type list
     * @tparam Templ template to apply types to
     *
     * e.g. apply_t<type_list<int>, std::vector> = std::vector<int>
     */
    template<typename TL, template<typename ...> typename Templ>
    struct apply;

    template<typename ...Ts, template<typename ...> typename Templ>
    struct apply<type_list<Ts...>, Templ> {
        using type = Templ<Ts...>;
    };

    template<typename TL, template<typename ...> typename Templ>
    using apply_t = typename apply<TL, Templ>::type;


	/**
     * Transform each element in the type list to a new type.
     *
     * @tparam TL type list
     * @tparam func the transform function. Must be invocable as INVOKE(func, std::type_identity<T>{}) for all T in the type list
     *      and must return a type with a member typedef (called `type`).
     *
     * @example
     * @code
     * using tl = type_list<int, double>;
     *
     * using transformed_tl = transform_t<tl, []<typename T>(std::type_identity<T>) {
     *     return std::add_const<T>{};
     * }>;
     *
     * static_assert(std::is_same_v<transformed_tl, type_list<int const, double const>>);
     * @endcode
     */
    template<typename TL, auto func>
    struct transform;

    template<typename ...Ts, auto func>
    struct transform<type_list<Ts...>, func> {
        using type = type_list<typename decltype(std::invoke(func, std::type_identity<Ts>{}))::type...>;
    };

    template<typename TL, auto func>
    using transform_t = typename transform<TL, func>::type;


	/**
     * Filter the types in the type list based on a predicate.
     *
     * @tparam TL type list
     * @tparam pred the predicate to filter the types. Must be invocable as INVOKE<bool>(func, std::type_identity<T>{}) for all T in the type list.
     */
    template<typename TL, auto pred>
    struct filter;

    template< auto pred>
    struct filter<type_list<>, pred> {
        using type = type_list<>;
    };

    template<typename T, typename ...Ts, auto pred>
    requires (std::invoke(pred, std::type_identity<T>{}))
    struct filter<type_list<T, Ts...>, pred> {
        using type = concat_t<type_list<T>, typename filter<type_list<Ts...>, pred>::type>;
    };

    template<typename T, typename ...Ts, auto pred>
    requires (!std::invoke(pred, std::type_identity<T>{}))
    struct filter<type_list<T, Ts...>, pred> {
        using type = typename filter<type_list<Ts...>, pred>::type;
    };

    template<typename TL, auto pred>
    using filter_t = typename filter<TL, pred>::type;


    namespace detail_generate {
        template<typename IdxSeq, auto func>
        struct generate_impl;

        template<size_t ...idxs, auto func>
        struct generate_impl<std::index_sequence<idxs...>, func> {
            using type = type_list<typename decltype(std::invoke(func, std::integral_constant<size_t, idxs>{}))::type...>;
        };
    } // namespace detail_generate

	/**
     * Generate a type list by repeatedly calling func with in index in 0..count (exclusive).
     *
     * @tparam count number of times to call func
     * @tparam func function to generate types.
     *      Must be invocable as INVOKE(func, std::integral_constant<size_t, idx>{})
     *      and must return a type with a member typedef (called `type`)
     */
    template<size_t count, auto func>
    struct generate {
        using type = typename detail_generate::generate_impl<std::make_index_sequence<count>, func>::type;
    };

    template<size_t count, auto func>
    using generate_t = typename generate<count, func>::type;


	/**
     * Find the first element for which INVOKE(pred, std::type_identity<T>{}) returns true, if any.
     * In case no element is found the member typedef `type` is not present; if this is undesirable use `opt`.
     *
     * @tparam TL type list
     * @tparam pred predicate to find type. Must be invocable as INVOKE<bool>(pred, std::type_identity<T>{}).
     */
    template<typename TL, auto pred>
    struct find_if;

    template<auto pred>
    struct find_if<type_list<>, pred> {
    };

    template<typename T, typename ...Ts, auto pred>
    requires (!std::invoke(pred, std::type_identity<T>{}))
    struct find_if<type_list<T, Ts...>, pred> : find_if<type_list<Ts...>, pred> {
    };

    template<typename T, typename ...Ts, auto pred>
    requires (std::invoke(pred, std::type_identity<T>{}))
    struct find_if<type_list<T, Ts...>, pred> {
        using type = T;
    };

    template<typename TL, auto pred>
    using find_if_t = typename find_if<TL, pred>::type;


    namespace detail_position {
        template<typename TL, auto pred, size_t ix>
        struct position;

        template<auto pred, size_t ix>
        struct position<type_list<>, pred, ix> {
        };

        template<typename T, typename ...Ts, auto pred, size_t ix>
        requires (!std::invoke(pred, std::type_identity<T>{}))
        struct position<type_list<T, Ts...>, pred, ix> : position<type_list<Ts...>, pred, ix + 1> {
        };

        template<typename T, typename ...Ts, auto pred, size_t ix>
        requires (std::invoke(pred, std::type_identity<T>{}))
        struct position<type_list<T, Ts...>, pred, ix> : std::integral_constant<size_t, ix> {
        };
    } // namespace detail_position

    /**
     * Searches for an element in the type list, returning the first position where the predicate returns true.
     */
    template<typename TL, auto pred>
    using position = detail_position::position<TL, pred, 0>;

    template<typename TL, auto pred>
    inline constexpr size_t position_v = position<TL, pred>::value;


	/**
     * Count the number of occurrences of a type in the type list
     *
     * @tparam TL type list
     * @tparam Needle type to count occurrences of
     */
    template<typename TL, typename Needle>
	struct count;

	template<typename Needle>
	struct count<type_list<>, Needle> : std::integral_constant<size_t, 0> {
	};

	template<typename ...Ts, typename Needle>
	struct count<type_list<Needle, Ts...>, Needle> : std::integral_constant<size_t, 1 + count<type_list<Ts...>, Needle>::value> {
	};

	template<typename T, typename ...Ts, typename Needle>
	struct count<type_list<T, Ts...>, Needle> : std::integral_constant<size_t, count<type_list<Ts...>, Needle>::value> {
	};

	template<typename TL, typename Needle>
	static constexpr size_t count_v = count<TL, Needle>::value;


	/**
     * Check if a particular type is present in the type list.
     *
     * @tparam TL type list
     * @tparam Needle type to search for
     */
    template<typename TL, typename Needle>
    struct contains;

    template<typename ...Ts, typename Needle>
    struct contains<type_list<Ts...>, Needle> : std::bool_constant<(std::is_same_v<Ts, Needle> || ...)> {
    };

    template<typename TL, typename Needle>
    inline constexpr bool contains_v = contains<TL, Needle>::value;


	/**
     * Check if all types in the type list fulfill a predicate.
     *
     * @tparam TL type list
     * @tparam pred predicate to apply to types. Must be invocable as INVOKE<bool>(pred, std::type_identity<T>{}) for all types in the type list.
     */
    template<typename TL, auto pred>
    struct all_of;

    template<typename ...Ts, auto pred>
    struct all_of<type_list<Ts...>, pred> : std::bool_constant<(std::invoke(pred, std::type_identity<Ts>{}) && ...)> {
    };

    template<typename TL, auto pred>
    inline constexpr bool all_of_v = all_of<TL, pred>::value;


    /**
     * Check if any of the types in the type list fulfill a predicate.
     *
     * @tparam TL type list
     * @tparam pred predicate to apply to types. Must be invocable as INVOKE<bool>(pred, std::type_identity<T>{}) for all types in the type list.
     */
    template<typename TL, auto pred>
    struct any_of;

    template<typename ...Ts, auto pred>
    struct any_of<type_list<Ts...>, pred> : std::bool_constant<(std::invoke(pred, std::type_identity<Ts>{}) || ...)> {
    };

    template<typename TL, auto pred>
    inline constexpr bool any_of_v = any_of<TL, pred>::value;


    /**
     * Check if no types in the type list fulfill a predicate.
     *
     * @tparam TL type list
     * @tparam pred predicate to apply to types. Must be invocable as INVOKE<bool>(pred, std::type_identity<T>{}) for all types in the type list.
     */
    template<typename TL, auto pred>
    struct none_of;

    template<typename ...Ts, auto pred>
    struct none_of<type_list<Ts...>, pred> : std::bool_constant<(!std::invoke(pred, std::type_identity<Ts>{}) && ...)> {
    };

    template<typename TL, auto pred>
    inline constexpr bool none_of_v = none_of<TL, pred>::value;


	/**
     * Check if all types in the type list are the same type.
     *
     * @tparam TL type list
     */
    template<typename TL>
    struct all_same;

    template<>
    struct all_same<type_list<>> : std::true_type {
    };

    template<typename T, typename ...Ts>
    struct all_same<type_list<T, Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> && ...)> {
    };

    template<typename TL>
    inline constexpr bool all_same_v = all_same<TL>::value;


	/**
     * Turn a std::integer_sequence into a type list of std::integral_constants
     */
    template<typename ISeq>
    struct integer_sequence_to_type_list;

    template<typename Int, Int ...ints>
    struct integer_sequence_to_type_list<std::integer_sequence<Int, ints...>> {
        using type = type_list<std::integral_constant<Int, ints>...>;
    };

    template<typename ISeq>
    using integer_sequence_to_type_list_t = typename integer_sequence_to_type_list<ISeq>::type;


	/**
     * A marker type to indicate a failed operation
     * when using opt
     */
    struct nullopt {
        constexpr bool operator==(nullopt const &other) const noexcept = default;
    };


    namespace detail_opt {

        template<typename T>
        concept type_present = requires {
            typename T::type;
        };

        template<typename T>
        concept value_present = requires {
            T::value;
        };

    } // namespace detail_opt

	/**
     * Can be used in combination with any operation that may fail.
     * By default, failing operations will not provide a `type` member typedef / a static constexpr value.
     * When using opt, the member typedef `type` will always be present. In case the operation failed the type will be `nullopt`.
     * Additionally, the static constexpr value will always be present. In case the operation failed the static constexpr value will be `nullopt{}`
     *
     * @example
     * @code
     * using X = first_t<type_list<>>; // compilation failure, first has no `type` typedef
     * using X = opt_t<first<type_list<>>; // compilation success, X is nullopt
     * @endcode
     */
    template<typename T>
    struct opt {
        using type = nullopt;
        static constexpr auto value = nullopt{};
    };

    template<typename T>
    requires (detail_opt::type_present<T> && !detail_opt::value_present<T>)
    struct opt<T> {
        using type = typename T::type;
        static constexpr auto value = nullopt{};
    };

    template<typename T>
    requires (!detail_opt::type_present<T> && detail_opt::value_present<T>)
    struct opt<T> {
        using type = nullopt;
        static constexpr auto value = T::value;
    };

    template<typename T>
    requires (detail_opt::type_present<T> && detail_opt::value_present<T>)
    struct opt<T> {
        using type = typename T::type;
        static constexpr auto value = T::value;
    };

    template<typename T>
    using opt_t = typename opt<T>::type;

    template<typename T>
    inline constexpr auto opt_v = opt<T>::value;


    namespace detail_for_each {
        template<typename ...Ts, typename F>
        constexpr void for_each_impl(type_list<Ts...>, F &&func) {
            (std::invoke(func, std::type_identity<Ts>{}), ...);
        }
    } // namespace detail_for_each

    /**
     * Apply a function to each type in a type list.
     * The function will be invoked with `std::type_identity<T>{}` for each type.
     */
    template<typename TL, typename F>
    constexpr void for_each(F &&func) {
        detail_for_each::for_each_impl(TL{}, std::forward<F>(func));
    }

    namespace detail_fold {
        template<typename Acc, typename ...Ts, typename F>
        [[nodiscard]] constexpr Acc fold_impl(type_list<Ts...>, Acc init, F &&func) {
            ((init = std::invoke(func, std::move(init), std::type_identity<Ts>{})), ...);
            return init;
        }
    } // namespace detail_fold

	/**
     * Fold over the types in a type list.
     * Repeatedly calls init = INVOKE(func, init, std::type_identity<T>{}) for all types in the type list
     * and returns init.
     *
     * @param init initial value of the accumulator
     * @param func function to fold types of the type list into the accumulator
     * @return the final value of the accumulator
     */
    template<typename TL, typename Acc, typename F>
    [[nodiscard]] constexpr Acc fold(Acc init, F &&func) {
        return detail_fold::fold_impl(TL{}, std::move(init), std::forward<F>(func));
    }

} // namespace dice::template_library::type_list

#endif // DICE_TEMPLATELIBRARY_TYPELIST_HPP
