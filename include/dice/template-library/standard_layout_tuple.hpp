#ifndef DICE_TEMPLATELIBRARY_STANDARDLAYOUTTUPLE_HPP
#define DICE_TEMPLATELIBRARY_STANDARDLAYOUTTUPLE_HPP

#include <dice/template-library/type_list.hpp>
#include <dice/template-library/type_traits.hpp>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace dice::template_library {

	/**
	 * A tag type to tell a tuple to construct all fields with the same arguments.
	 *
	 * @example
	 * @code
	 * standard_layout_tuple<int, int, int> const uniform{uniform_construct, 42};
	 * assert(uniform == standard_layout_tuple<int, int, int>{42, 42, 42});
	 * @endcode
	 */
	struct uniform_construct_t {};
	inline constexpr uniform_construct_t uniform_construct;

	/**
	 * A tag type to tell a tuple to construct each member with one of the given arguments.
	 * This mostly exists for backwards compatibility purposes and clarity. It is the same as the regular constructor.
	 *
	 * @example
	 * @code
	 * standard_layout_tuple<int, int, int> const individual{individual_construct, 1, 2, 3};
	 * assert(individual == standard_layout_tuple<int, int, int>{1, 2, 3});
	 * @encode
	 */
	struct individual_construct_t {};
	inline constexpr individual_construct_t individual_construct;

	/**
	 * A tuple that fulfills std::is_standard_layout
	 */
	template<typename ...Ts>
	struct standard_layout_tuple;

	template<>
	struct standard_layout_tuple<> {
		constexpr standard_layout_tuple() noexcept = default;
		explicit constexpr standard_layout_tuple(individual_construct_t) noexcept {}

		template<typename ...Args>
		explicit constexpr standard_layout_tuple(uniform_construct_t, [[maybe_unused]] Args &&...args) noexcept {}

		static constexpr size_t size() noexcept {
			return 0;
		}

		template<size_t offset, size_t count>
		[[nodiscard]] standard_layout_tuple const &subtuple() const noexcept {
			static_assert(offset == 0 && count == 0);
			return *this;
		}

		constexpr auto operator<=>(standard_layout_tuple const &other) const noexcept = default;
		constexpr bool operator==(standard_layout_tuple const &other) const noexcept = default;
	};

	template<typename T, typename ...Ts>
	struct standard_layout_tuple<T, Ts...> {
	private:
		T first;
		[[no_unique_address]] standard_layout_tuple<Ts...> rest;

	public:
		constexpr standard_layout_tuple() = default;

		template<typename ...Args>
		explicit constexpr standard_layout_tuple(Args &&...args) requires (sizeof...(Args) == 1 + sizeof...(Ts))
			: standard_layout_tuple{individual_construct, std::forward<Args>(args)...}
		{
		}

		template<typename Arg, typename ...Args>
		explicit constexpr standard_layout_tuple(individual_construct_t, Arg &&first, Args &&...rest) requires (sizeof...(Ts) == sizeof...(Args))
			: first{std::forward<Arg>(first)},
			  rest{std::forward<Args>(rest)...}
		{
		}

		template<typename ...Args>
		explicit constexpr standard_layout_tuple(uniform_construct_t, Args &&...args)
			: first{args...},
			  rest{uniform_construct, std::forward<Args>(args)...}
		{
		}

		[[nodiscard]] static constexpr size_t size() noexcept {
			return 1 + sizeof...(Ts);
		}

		template<size_t ix, typename Self>
		[[nodiscard]] constexpr decltype(auto) get(this Self &&self) noexcept {
			static_assert(ix < size(), "Index for get must be in range");

			if constexpr (ix == 0) {
				return dice::template_library::forward_like<Self>(self.first);
			} else {
				return dice::template_library::forward_like<Self>(self.rest).template get<ix - 1>();
			}
		}

		template<typename U, typename Self>
		[[nodiscard]] constexpr decltype(auto) get(this Self &&self) noexcept {
			using types = type_list::type_list<T, Ts...>;
			static_assert(type_list::count_v<types, U> == 1,
						  "Type must be present exactly once in the tuple");

			static constexpr size_t index = type_list::position_v<types, []<typename V>(std::type_identity<V>) {
				return std::is_same_v<U, V>;
			}>;

			return std::forward<Self>(self).template get<index>();
		}

		template<size_t offset, size_t count = size(), typename Self>
		[[nodiscard]] decltype(auto) subtuple(this Self &&self) noexcept {
			static_assert(std::is_standard_layout_v<standard_layout_tuple>,
						  "Types inhabiting the standard_layout_tuple must be standard layout for subtuple to work correctly.");
			static_assert(offset < size() && count <= (size() - offset),
						  "Indices for subtuple must be in range");

			if constexpr (offset == 0) {
				using types = type_list::type_list<T, Ts...>;
				using subtypes = type_list::take_t<types, count>;
				using new_tuple = type_list::apply_t<subtypes, standard_layout_tuple>;

				// SAFETY: ensured by being a standard layout type
				return reinterpret_cast<copy_cvref_t<decltype(std::forward<Self>(self)), new_tuple>>(self);
			} else {
				return dice::template_library::forward_like<Self>(self.rest).template subtuple<offset - 1, count>();
			}
		}

		constexpr auto operator<=>(standard_layout_tuple const &other) const noexcept = default;
		constexpr bool operator==(standard_layout_tuple const &other) const noexcept = default;
	};

} // namespace dice::template_library

template<typename ...Ts>
struct std::tuple_size<dice::template_library::standard_layout_tuple<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {
};

template<size_t ix, typename ...Ts>
struct std::tuple_element<ix, dice::template_library::standard_layout_tuple<Ts...>> {
	using type = dice::template_library::type_list::nth_t<dice::template_library::type_list::type_list<Ts...>, ix>;
};

#endif // DICE_TEMPLATELIBRARY_STANDARDLAYOUTTUPLE_HPP
