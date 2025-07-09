#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/memfn.hpp>

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

TEST_SUITE("DICE_MEMFN Argument Forwarding") {

	struct AbstractFixture {
		enum struct CallSignature : uint8_t {
			None,
			ByValue,
			ByConstLvalueRef,
			ByLvalueRef,
			ByRvalueRef,
			MixedArgs,
			RangeTransform
		};
		CallSignature last_call = CallSignature::None;
		std::string moved_in_data;

		// Member functions to be called by the macro
		void process_by_value(std::string s) { last_call = CallSignature::ByValue; }
		void process_by_const_lvalue_ref(std::string const &s) { last_call = CallSignature::ByConstLvalueRef; }
		void process_by_lvalue_ref(std::string &s) {
			last_call = CallSignature::ByLvalueRef;
			s += "_modified";
		}
		void process_by_rvalue_ref(std::string &&s) {
			last_call = CallSignature::ByRvalueRef;
			moved_in_data = std::move(s);
		}
		void process_mixed_args(std::string const &s_value, std::string const &s_const_ref, int &i_ref) {
			last_call = CallSignature::MixedArgs;
			i_ref *= 2;
			moved_in_data = s_const_ref + "_" + s_value;
		}
		std::string transform_int_to_string(std::string const &prefix, int i) {
			last_call = CallSignature::RangeTransform;
			return prefix + std::to_string(i);
		}


		// methods for the tests
		void run_const_lvalue_ref_test() {
			std::string const data = "immutable";
			auto task = DICE_MEMFN(process_by_const_lvalue_ref);
			task(data);
			REQUIRE(last_call == CallSignature::ByConstLvalueRef);
		}

		void run_lvalue_ref_test() {
			std::string data = "mutable";
			auto task = DICE_MEMFN(process_by_lvalue_ref);
			task(data);
			REQUIRE(last_call == CallSignature::ByLvalueRef);
			REQUIRE(data == "mutable_modified");
		}

		void run_rvalue_ref_test() {
			std::string data = "movable";
			auto task = DICE_MEMFN(process_by_rvalue_ref);
			task(std::move(data));
			REQUIRE(last_call == CallSignature::ByRvalueRef);
			REQUIRE(moved_in_data == "movable");
			REQUIRE(data.empty());
		}

		void run_by_value_test() {
			std::string data = "copyable";
			auto task = DICE_MEMFN(process_by_value);

			task(data);
			REQUIRE(last_call == CallSignature::ByValue);
			REQUIRE(data == "copyable");

			task("temporary");
			REQUIRE(last_call == CallSignature::ByValue);
		}

		void run_mixed_args_test() {
			std::string const const_data = "hello";
			int mutable_int = 10;
			std::string movable_data = "world";

			auto task = DICE_MEMFN(process_mixed_args, movable_data);
			task(const_data, mutable_int);

			REQUIRE(last_call == CallSignature::MixedArgs);
			REQUIRE(mutable_int == 20);
			REQUIRE(moved_in_data == "hello_world");
			REQUIRE_FALSE(movable_data.empty());
		}

		void run_range_adaptor_test() {
			std::vector<int> source_data = {10, 20, 30};
			std::string const prefix = "item_";

			auto transformed_view = source_data | std::views::transform(DICE_MEMFN(transform_int_to_string, prefix));

			std::vector<std::string> results;
			std::ranges::copy(transformed_view, std::back_inserter(results));

			REQUIRE(last_call == CallSignature::RangeTransform);

			REQUIRE(results.size() == 3);
			REQUIRE(results[0] == "item_10");
			REQUIRE(results[1] == "item_20");
			REQUIRE(results[2] == "item_30");
		}
	};

	TEST_CASE("Forwards arguments as const lvalue references") {
		AbstractFixture f;
		f.run_const_lvalue_ref_test();
	}

	TEST_CASE("Forwards arguments as non-const lvalue references") {
		AbstractFixture f;
		f.run_lvalue_ref_test();
	}

	TEST_CASE("Forwards arguments as rvalue references") {
		AbstractFixture f;
		f.run_rvalue_ref_test();
	}

	TEST_CASE("Forwards arguments by value") {
		AbstractFixture f;
		f.run_by_value_test();
	}

	TEST_CASE("Forwards mixed arguments with pre-binding") {
		AbstractFixture f;
		f.run_mixed_args_test();
	}

	TEST_CASE("Usage with a range adaptor") {
		AbstractFixture f;
		f.run_range_adaptor_test();
	}
}