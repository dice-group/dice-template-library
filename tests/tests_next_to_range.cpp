#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/next_to_range.hpp>


#include <algorithm>
#include <cstddef>
#include <initializer_list>
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

		[[nodiscard]] std::optional<int> next() {
			return cur_++;
		}
	};

	using non_copy_iota = dtl::next_to_range<non_copy_iota_iter>;
	static_assert(std::ranges::range<non_copy_iota>);
	static_assert(!std::ranges::sized_range<non_copy_iota>);

	struct values_yielder_iter {
		using value_type = int;

	private:
		std::vector<int> values_;
		size_t cur_;
		size_t end_;

	public:
		explicit values_yielder_iter(std::initializer_list<int> values) : values_{values}, cur_{0}, end_{values_.size()} {
		}

		[[nodiscard]] std::optional<int> next() {
			if (cur_ >= end_) {
				return std::nullopt;
			}

			return values_[cur_++];
		}

		[[nodiscard]] std::optional<int> nth(size_t off) {
			if (off > remaining()) {
				cur_ = end_;
				return std::nullopt;
			}

			cur_ += off;
			return next();
		}

		[[nodiscard]] std::optional<int> next_back() {
			if (cur_ >= end_) {
				return std::nullopt;
			}

			return values_[--end_];
		}

		[[nodiscard]] std::optional<int> nth_back(size_t off) {
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

	using values_yielder = dtl::next_to_range<values_yielder_iter>;
	static_assert(std::ranges::range<values_yielder>);
	static_assert(std::ranges::sized_range<values_yielder>);


	TEST_CASE("sanity check") {
		non_copy_iota ints{0};
		CHECK(std::ranges::equal(ints | std::views::take(3), std::vector<int>{0, 1, 2}));

		auto even_ints = ints
			| std::views::transform([](int x) { return x + 1; })
			| std::views::filter([](int x) { return x % 2 == 0; })
			| std::views::take(3);

		CHECK(std::ranges::equal(even_ints, std::vector<int>{2, 4, 6}));
	}

	TEST_CASE("peeking") {
		values_yielder ints{1, 2};

		CHECK_FALSE(ints.empty());
		CHECK_EQ(ints.size(), 2);
		CHECK(ints);

		CHECK_EQ(ints.front(), 1);

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
	}

	TEST_CASE("reverse iterator") {
		values_yielder ints{1, 2};
		auto rev = ints.reversed();

		CHECK(std::ranges::equal(rev, std::vector{2, 1}));

		CHECK_EQ(ints.front(), 1);
		CHECK_EQ(ints.back(), 2);
	}

	TEST_CASE("empty") {
		values_yielder empty{};

		CHECK_EQ(empty.size(), 0);
		CHECK(empty.empty());
		CHECK_FALSE(empty);
	}

	TEST_CASE("random access") {
		values_yielder ints{0, 1, 2, 3};
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

	TEST_CASE("postincrement") {
		SUBCASE("non-copyable") {
			non_copy_iota const ints{0};
			auto iter = ints.begin();

			static_assert(std::same_as<decltype(iter++), void>);

			CHECK_EQ(*iter, 0);
			iter++;
			CHECK_EQ(*iter, 1);
		}

		SUBCASE("copyable") {
			values_yielder const ints{0, 1};
			auto iter = ints.begin();

			static_assert(std::same_as<decltype(iter++), typename values_yielder::iterator>);

			CHECK_EQ(*iter, 0);

			auto cpy = iter++;
			CHECK_EQ(*cpy, 0);
			CHECK_EQ(*iter, 1);
		}
	}
}
