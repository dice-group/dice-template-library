#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <numeric>

#include <dice/template-library/static_vector.hpp>

namespace dice::template_library {
	TEST_SUITE("static vector") {
		TEST_CASE("test push pop") {
			static_vector<int, 5> v;
			CHECK(v.empty());

			v.push_back(1);
			v.push_back(2);
			v.push_back(3);
			v.push_back(4);
			v.push_back(5);
			CHECK_THROWS(v.push_back(6));
			CHECK(v.size() == 5);
			v.pop_back();
			v.pop_back();
			CHECK(v.size() == 3);
			v.push_back(99);
			v.push_back(100);
			CHECK_THROWS(v.push_back(101));
			CHECK(v.size() == 5);
		}

		TEST_CASE("emplace") {
			struct non_copy_move_t {
				int i;
				double d;

				non_copy_move_t() = default;
				non_copy_move_t(int i, double d) : i{i}, d{d} {}

				non_copy_move_t(non_copy_move_t const &other) { throw std::runtime_error{"unexpected copy"}; }
				non_copy_move_t(non_copy_move_t &&other) { throw std::runtime_error{"unexpected move"}; }

				bool operator==(non_copy_move_t const &) const = default;
			};

			static_vector<non_copy_move_t, 1> v;
			auto &ref = v.emplace_back(1, 2.0);
			CHECK(ref == non_copy_move_t{1, 2.0});

			CHECK_THROWS(v.emplace_back(1, 2.0));
		}

		TEST_CASE("iterators positions") {
			static_vector<int, 5> const v{1, 2, 3};
			CHECK(v.rend().operator->() == v.data() - 1);
			CHECK(v.rbegin().operator->() == v.data() + v.size() - 1);

			CHECK(v.begin() == v.data());
			CHECK(v.end() == v.data() + v.size());
		}

		TEST_CASE("use with std:: algorithms") {
			static_vector<int, 10> const v{1, 2, 3, 4, 5};

			int const sum = std::accumulate(v.begin(), v.end(), 0, std::plus<>{});
			CHECK(sum == 15);

			auto const it = std::find(v.rbegin(), v.rend(), 3);
			CHECK(it == v.rbegin() + 2);
			CHECK(*it == 3);
		}

		TEST_CASE("use with std::ranges:: algorithms") {
			static_vector<int, 10> const v{1, 2, 3, 4, 5};
			static_vector<int, 5> v2;

			std::ranges::transform(v, std::back_inserter(v2), [](auto const x) { return x + 5; });

			bool const eq = std::ranges::equal(v2, std::initializer_list<int>{6, 7, 8, 9, 10});
			CHECK(eq);
		}

		TEST_CASE("check iter correctly bounded") {
			static_vector<int, 10> v{1, 2, 3};
			CHECK(v.size() == 3);

			size_t n = 0;
			for (auto const &x : v) {
				n += 1;
			}

			CHECK(n == 3);
		}

		TEST_CASE("direct access") {
			static_vector<double, 5> v{1, 2, 3, 4};

			CHECK(v[1] == 2);
			CHECK(v[3] == 4);

			double discard;
			CHECK_THROWS(discard = v.at(4));

			v[1] = 99;
			CHECK(v[1] == 99);

			CHECK(v.front() == 1);
			CHECK(v.back() == 4);
		}
	}
}// namespace dice::template_library