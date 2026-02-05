#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/next_to_range.hpp>


#include <algorithm>
#include <optional>
#include <ranges>
#include <vector>

namespace dtl = dice::template_library;

TEST_SUITE("next_to_range") {
	struct non_copy_iota_iter {
		using value_type = int;

	private:
		int cur_;

	public:
		explicit non_copy_iota_iter(int start = 0) noexcept : cur_{start} {
		}

		non_copy_iota_iter(non_copy_iota_iter &&other) = default;
		non_copy_iota_iter &operator=(non_copy_iota_iter &&other) = default;
		non_copy_iota_iter(non_copy_iota_iter const &other) = delete;
		non_copy_iota_iter &operator=(non_copy_iota_iter const &other) = delete;
		~non_copy_iota_iter() = default;

	protected:
		[[nodiscard]] std::optional<value_type> next() {
			return cur_++;
		}
	};

	using non_copy_iota = dtl::next_to_range<non_copy_iota_iter>;
	static_assert(std::ranges::range<non_copy_iota>);
	static_assert(!std::ranges::view<non_copy_iota>);
	static_assert(!std::ranges::sized_range<non_copy_iota>);

	template<typename Container>
	struct values_yielder_iter {
		using value_type = typename Container::value_type;

	private:
		Container values_;
		size_t cur_;
		size_t end_;

	public:
		explicit values_yielder_iter(Container values = {}) : values_{std::move(values)}, cur_{0}, end_{values_.size()} {
		}

		[[nodiscard]] std::optional<value_type> next() {
			if (cur_ >= end_) {
				return std::nullopt;
			}

			return values_[cur_++];
		}

		[[nodiscard]] std::optional<value_type> nth(size_t off) {
			if (off > remaining()) {
				cur_ = end_;
				return std::nullopt;
			}

			cur_ += off;
			return next();
		}

		[[nodiscard]] std::optional<value_type> next_back() {
			if (cur_ >= end_) {
				return std::nullopt;
			}

			return values_[--end_];
		}

		[[nodiscard]] std::optional<value_type> nth_back(size_t off) {
			if (off > remaining()) {
				end_ = cur_;
				return std::nullopt;
			}

			end_ -= off;
			return next_back();
		}

		[[nodiscard]] size_t remaining() const noexcept {
			return end_ - cur_;
		}
	};

	using values_range = dtl::next_to_range<values_yielder_iter<std::vector<int>>>;
	static_assert(std::ranges::range<values_range>);
	static_assert(std::ranges::sized_range<values_range>);
	static_assert(!std::ranges::view<values_range>);

	using values_view = dtl::next_to_view<values_yielder_iter<std::span<int const>>>;
	static_assert(std::ranges::range<values_view>);
	static_assert(std::ranges::sized_range<values_view>);
	static_assert(std::ranges::view<values_view>);

	TEST_CASE("sanity check") {
		non_copy_iota ints{0};
		CHECK(std::ranges::equal(ints | std::views::take(3), std::vector<int>{0, 1, 2}));

		auto even_ints = ints
			| std::views::transform([](int x) { return x + 1; })
			| std::views::filter([](int x) { return x % 2 == 0; })
			| std::views::take(3);

		CHECK(std::ranges::equal(even_ints, std::vector<int>{2, 4, 6}));
	}

	TEST_CASE_TEMPLATE("peeking", R, values_range, values_view) {
		std::vector vec{1, 2};
		R ints{vec};

		CHECK_FALSE(ints.empty());
		CHECK_EQ(ints.size(), 2);
		CHECK(ints);

		auto iter = ints.begin();
		CHECK_NE(iter, ints.end());
		CHECK_EQ(*iter, 1);
		CHECK_EQ(iter.peek(), 2);

		++iter;

		CHECK_NE(iter, ints.end());
		CHECK_EQ(*iter, 2);
		CHECK_EQ(iter.peek(), std::nullopt);

		++iter;
		CHECK_EQ(iter, ints.end());
		CHECK_EQ(iter.peek(), std::nullopt);

		if constexpr (std::ranges::view<R>) {
			CHECK_EQ(ints.front(), 1);
			CHECK_EQ(ints.back(), 2);
		}
	}

	TEST_CASE_TEMPLATE("reverse iterator", R, values_range, values_view) {
		std::vector vec{1, 2};
		R ints{vec};
		auto rev = ints.reversed();

		CHECK(std::ranges::equal(rev, std::vector{2, 1}));
	}

	TEST_CASE_TEMPLATE("empty", R, values_range, values_view) {
		R empty{};

		CHECK_EQ(empty.size(), 0);
		CHECK(empty.empty());
		CHECK_FALSE(empty);
	}

	TEST_CASE_TEMPLATE("random access", R, values_range, values_view) {
		std::vector vec{0, 1, 2, 3};
		R ints{vec};
		auto it = ints.begin();

		CHECK_EQ(*it, 0);
		CHECK_EQ(it.peek(), 1);
		CHECK_EQ(*(it + 1), 1);
		CHECK_EQ(it.peek(), 1);
		CHECK_EQ(*(it += 1), 1);
		CHECK_EQ(it.peek(), 2);
		CHECK_EQ(*(it += 2), 3);
		CHECK_EQ(it.peek(), std::nullopt);

		++it;
		CHECK_EQ(it, ints.end());

		if constexpr (std::ranges::view<R>) {
			CHECK_EQ(ints[0], 0);
			CHECK_EQ(ints[1], 1);
			CHECK_EQ(ints[2], 2);
			CHECK_EQ(ints[3], 3);

			std::ranges::equal(ints, std::vector{0, 1, 2, 3});

			ints.advance(0);
			CHECK(std::ranges::equal(ints, std::vector{0, 1, 2, 3}));

			ints.advance(2);
			CHECK(std::ranges::equal(ints, std::vector{2, 3}));

			ints.advance(1);
			CHECK(std::ranges::equal(ints, std::vector{3}));

			ints.advance(1);
			CHECK(std::ranges::equal(ints, std::vector<int>{}));
		}
	}

	TEST_CASE("postincrement, non-copyable") {
		SUBCASE("non-copyable") {
			non_copy_iota const ints{0};
			auto iter = ints.begin();

			static_assert(std::same_as<decltype(iter++), void>);

			CHECK_EQ(*iter, 0);
			iter++;
			CHECK_EQ(*iter, 1);
		}
	}

	TEST_CASE_TEMPLATE("postincrement, copyable", R, values_range, values_view) {
		std::vector vec{0, 1};
		R const ints{vec};
		auto iter = ints.begin();

		static_assert(std::same_as<decltype(iter++), typename R::iterator>);

		CHECK_EQ(*iter, 0);

		auto cpy = iter++;
		CHECK_EQ(*cpy, 0);
		CHECK_EQ(*iter, 1);
	}

	TEST_CASE_TEMPLATE("pipeline pitfall", R, values_range, values_view) {
		std::vector<int> vec{1, 2, 3};

		auto ints = R{vec} | std::views::transform([](auto const &x) { return x + 1; });

		if constexpr (std::is_same_v<R, values_view>) {
			// This does not work with values_range because it is not a view,
			// and since it is passed as a temporary into std::views::transform the standard library wraps it
			// in std::ranges::owning_view which is not copy constructable.
			// Then, take tries to copy it but can't, resulting in a compiler error.
			// With values view this works fine, because it is marked as a (cheaply copyable) view.
			auto first = ints | std::views::take(1);
			CHECK(std::ranges::equal(first, std::vector<int>{2}));
		}

		CHECK(std::ranges::equal(ints, std::vector<int>{2, 3, 4}));
	}

	TEST_CASE_TEMPLATE("non-const element references", R, values_range, values_view) {
		std::vector<int> vec{1, 2, 3};
		R const ints{vec};

		for (int &x : ints) {
			// just testing if this compiles with non-const reference
			CHECK(x > 0);
		}
	}
}
