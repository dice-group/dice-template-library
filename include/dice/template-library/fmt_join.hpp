#ifndef DICE_TEMPLATE_LIBRARY_FMT_JOIN_HPP
#define DICE_TEMPLATE_LIBRARY_FMT_JOIN_HPP

#include <format>
#include <iterator>
#include <ranges>

namespace dice::template_library {

	namespace fmt_join_algo_detail {
		template<std::ranges::input_range R, typename Sep>
		struct joiner {
			R range;
			Sep separator;
		};
	}// namespace fmt_join_algo_detail

	/**
	 * @brief Creates a formatting helper alike join from https://fmt.dev/latest/api/#range-and-tuple-formatting
	 *
	 * This function is to be used directly inside `std::format`. It does not
	 * create the final string itself but returns an internal helper object that `std::formatter`
	 * knows how to process.
	 *
	 * @param range The range of elements to join. The elements must be formattable.
	 * @param separator A string-like object (e.g., ", ") to place between elements.
	 * @return A helper object for use by `std::format`.
	 */
	template<std::ranges::input_range R, typename Sep>
	constexpr auto fmt_join(R &&range, Sep &&separator) {
		return fmt_join_algo_detail::joiner{std::views::all(std::forward<R>(range)), std::forward<Sep>(separator)};
	}

	template<typename T, typename Sep>
	constexpr auto fmt_join(std::initializer_list<T> range, Sep &&separator) {
		return fmt_join_algo_detail::joiner{std::views::all(std::vector(range)), std::forward<Sep>(separator)};
	}

}// namespace dice::template_library

// actual implementation of fmt_join
namespace std {
	template<typename R, typename Sep, typename CharT>
	struct formatter<dice::template_library::fmt_join_algo_detail::joiner<R, Sep>, CharT> {
	private:
		formatter<std::ranges::range_value_t<R>, CharT> element_formatter_;

	public:
		constexpr auto parse(auto &parse_ctx) {
			// forward parsing (e.g., `{:.2f}`) to element's formatter
			return element_formatter_.parse(parse_ctx);
		}

		auto format(const dice::template_library::fmt_join_algo_detail::joiner<R, Sep> &join_obj, auto &format_ctx) const {
			auto it = std::ranges::begin(join_obj.range);
			const auto end = std::ranges::end(join_obj.range);

			if (it == end) {
				return format_ctx.out();
			}

			// format the first element
			format_ctx.advance_to(element_formatter_.format(*it, format_ctx));
			++it;


			while (it != end) {
				// format separator
				if constexpr (std::ranges::range<const Sep &>) {
					format_ctx.advance_to(std::ranges::copy(join_obj.separator, format_ctx.out()));
				} else {
					// otherwise single character
					*format_ctx.out()++ = join_obj.separator;
				}

				// format next element
				format_ctx.advance_to(element_formatter_.format(*it, format_ctx));
				++it;
			}

			return format_ctx.out();
		}
	};

}// namespace std

#endif// DICE_TEMPLATE_LIBRARY_FMT_JOIN_HPP
