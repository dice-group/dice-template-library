#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/inplace_polymorphic.hpp>

TEST_SUITE("inplace_polymorphic") {

	TEST_CASE("Sanity check") {
		struct base {
			virtual ~base() = default;
			virtual void copy_to(base *dst) const = 0;
			virtual void move_to(base *dst) = 0;

			virtual void print() = 0;
		};

		struct derived1 : base {
			int x;

			explicit derived1(int x = 0) : x{x} {
			}

			void copy_to(base *dst) const override {
				new (dst) derived1{*this};
			}

			void move_to(base *dst) override {
				new (dst) derived1{std::move(*this)};
			}

			void print() override {
				std::cout << x << std::endl;
			}
		};

		struct derived2 : base {
			double x;

			explicit derived2(double x) : x{x} {
			}

			void copy_to(base *dst) const override {
				new (dst) derived2{*this};
			}

			void move_to(base *dst) override {
				new (dst) derived2{std::move(*this)};
			}

			void print() override {
				std::cout << x << std::endl;
			}
		};

		namespace dtl = dice::template_library;

		dtl::inplace_polymorphic<base, derived1, derived2> ip;


	}

}
