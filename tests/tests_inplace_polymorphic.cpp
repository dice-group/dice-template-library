#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/inplace_polymorphic.hpp>

#include <new>
#include <type_traits>
#include <utility>
#include <memory>

namespace dtl = dice::template_library;

TEST_SUITE("inplace_polymorphic: no copy, no move") {
	struct base {
		virtual ~base() = default;
		virtual double f() const = 0;
	};

	struct derived1 : base {
		int value;

		derived1() {
			throw std::runtime_error{"AAA"};
		}

		explicit derived1(int x) noexcept : value{x} {
		}

		double f() const override {
			return static_cast<double>(value);
		}
	};

	struct derived2 : base {
		double value;

		derived2() noexcept : value{42} {
		}

		explicit derived2(double x) noexcept : value{x} {
		}

		double f() const override {
			return value;
		}
	};

	using any = dtl::inplace_polymorphic<base, derived1, derived2>;
	using alt_any = dtl::inplace_polymorphic<base, derived2, derived1>;

	static_assert(!std::is_copy_constructible_v<any>);
	static_assert(!std::is_copy_assignable_v<any>);
	static_assert(!std::is_move_constructible_v<any>);
	static_assert(!std::is_move_assignable_v<any>);
	static_assert(std::is_default_constructible_v<any>);
	static_assert(std::is_nothrow_constructible_v<any, std::in_place_type_t<derived1>, int>);
	static_assert(std::is_nothrow_constructible_v<any, std::in_place_type_t<derived2>, double>);
	static_assert(std::is_default_constructible_v<alt_any>);
	static_assert(std::is_nothrow_default_constructible_v<alt_any>);

	inline void make_valueluess(any &obj) {
		try {
			obj.template emplace<derived1>();
			FAIL("did not throw");
		} catch (std::runtime_error const &) {
			// expected
		}
		CHECK(obj.valueless_by_exception());
	}

	TEST_CASE("throwing default constructor") {
		try {
			any deflt{};
			FAIL("did not throw");
		} catch (std::runtime_error const &) {
			// success
		}
	}

	TEST_CASE("getters") {
		any obj{std::in_place_type<derived1>, 42};
		CHECK_FALSE(obj.valueless_by_exception());
		CHECK_EQ(obj.get().f(), 42.0);
		CHECK_EQ(std::as_const(obj).get().f(), 42.0);
		CHECK_EQ((*obj).f(), 42.0);
		CHECK_EQ((*std::as_const(obj)).f(), 42.0);
		CHECK_EQ(obj->f(), 42.0);
		CHECK_EQ(std::as_const(obj)->f(), 42.0);

		make_valueluess(obj);

		CHECK(obj.valueless_by_exception());
		CHECK_THROWS((void) obj.get());
		CHECK_THROWS((void) std::as_const(obj).get());
		CHECK_THROWS((void) *obj);
		CHECK_THROWS((void) *std::as_const(obj));
		CHECK_THROWS((void) obj.operator->());
		CHECK_THROWS((void) std::as_const(obj).operator->());
	}

	TEST_CASE("emplace") {
		any obj{std::in_place_type<derived1>, 42};
		CHECK_FALSE(obj.valueless_by_exception());
		CHECK_EQ(obj->f(), 42);

		// change type
		obj.template emplace<derived2>(12.5);
		CHECK_FALSE(obj.valueless_by_exception());
		CHECK_EQ(obj->f(), 12.5);

		make_valueluess(obj);

		// construct new type after valueluess
		obj.template emplace<derived1>(56);
		CHECK_FALSE(obj.valueless_by_exception());
		CHECK_EQ(obj->f(), 56.0);
	}

	TEST_CASE("destroy valueluess") {
		any obj{std::in_place_type<derived1>, 42};
		make_valueluess(obj);
	}
}

TEST_SUITE("inplace_polymorphic: only copy") {
	struct base {
		virtual ~base() = default;
		virtual double f() const = 0;

		virtual void copy_to(base *dst) const = 0;
	};

	struct derived1 : base {
		int value;

		derived1() {
			throw std::runtime_error{"AAA"};
		}

		explicit derived1(int x) noexcept : value{x} {
		}

		double f() const override {
			return static_cast<double>(value);
		}

		void copy_to(base *dst) const override {
			new (dst) derived1{*this};
		}
	};

	struct derived2 : base {
		double value;

		derived2() noexcept : value{42} {
		}

		explicit derived2(double x) noexcept : value{x} {
		}

		double f() const override {
			return value;
		}

		void copy_to(base *dst) const override {
			new (dst) derived2{*this};
		}
	};

	using any = dtl::inplace_polymorphic<base, derived1, derived2>;

	static_assert(std::is_copy_constructible_v<any>);
	static_assert(std::is_copy_assignable_v<any>);

	inline void make_valueluess(any &obj) {
		try {
			obj.template emplace<derived1>();
			FAIL("did not throw");
		} catch (std::runtime_error const &) {
			// expected
		}
		CHECK(obj.valueless_by_exception());
	}

	TEST_CASE("copy ctor") {
		any obj{std::in_place_type<derived1>, 42};
		any obj2{obj};

		CHECK_FALSE(obj.valueless_by_exception());
		CHECK_FALSE(obj2.valueless_by_exception());
		CHECK_EQ(obj->f(), 42.0);
		CHECK_EQ(obj2->f(), 42.0);

		any valueluess{obj};
		make_valueluess(valueluess);
		CHECK(valueluess.valueless_by_exception());
		CHECK_THROWS((void) *valueluess);

		any valueluess2{valueluess};
		CHECK(valueluess2.valueless_by_exception());
		CHECK_THROWS((void) *valueluess2);
	}

	TEST_CASE("copy assignment") {
		any obj{std::in_place_type<derived1>, 42};
		any obj2{std::in_place_type<derived2>, 12.5};

		any valueluess{obj};
		make_valueluess(valueluess);

		CHECK_EQ(obj->f(), 42.0);
		CHECK_EQ(obj2->f(), 12.5);

		obj2 = obj;
		CHECK_EQ(obj2->f(), 42.0);

		obj2 = valueluess;
		CHECK(obj2.valueless_by_exception());
		CHECK_THROWS((void) *obj2);

		valueluess = obj;
		CHECK_FALSE(valueluess.valueless_by_exception());
		CHECK_EQ(valueluess->f(), 42.0);
	}
}

TEST_SUITE("inplace_polymorphic: only move") {
	struct base {
		virtual ~base() = default;
		virtual double f() const = 0;

		virtual void move_to(base *dst) = 0;
	};

	struct derived1 : base {
		std::unique_ptr<int> value;

		derived1() {
			throw std::runtime_error{"AAA"};
		}

		explicit derived1(int x) noexcept : value{std::make_unique<int>(x)} {
		}

		double f() const override {
			if (value == nullptr) {
				return INFINITY;
			}

			return static_cast<double>(*value);
		}

		void move_to(base *dst) override {
			new (dst) derived1{std::move(*this)};
		}
	};

	struct derived2 : base {
		std::unique_ptr<double> value;

		explicit derived2(double x) noexcept : value{std::make_unique<double>(x)} {
		}

		double f() const override {
			if (value == nullptr) {
				return INFINITY;
			}

			return *value;
		}

		void move_to(base *dst) override {
			new (dst) derived2{std::move(*this)};
		}
	};

	using any = dtl::inplace_polymorphic<base, derived1, derived2>;

	static_assert(!std::is_copy_constructible_v<any>);
	static_assert(!std::is_copy_assignable_v<any>);
	static_assert(std::is_move_constructible_v<any>);
	static_assert(std::is_move_assignable_v<any>);

	inline void make_valueluess(any &obj) {
		try {
			obj.template emplace<derived1>();
			FAIL("did not throw");
		} catch (std::runtime_error const &) {
			// expected
		}
		CHECK(obj.valueless_by_exception());
	}

	TEST_CASE("move ctor") {
		any obj{std::in_place_type<derived1>, 42};
		any obj2{std::move(obj)};

		CHECK_FALSE(obj.valueless_by_exception());
		CHECK_FALSE(obj2.valueless_by_exception());
		CHECK_EQ(obj->f(), INFINITY);
		CHECK_EQ(obj2->f(), 42.0);

		any valueluess{std::in_place_type<derived1>, 99};
		make_valueluess(valueluess);
		CHECK(valueluess.valueless_by_exception());
		CHECK_THROWS((void) *valueluess);

		any valueluess2{std::move(valueluess)};
		CHECK(valueluess.valueless_by_exception());
		CHECK(valueluess2.valueless_by_exception());
		CHECK_THROWS((void) *valueluess);
		CHECK_THROWS((void) *valueluess2);
	}

	TEST_CASE("move assignment") {
		any obj{std::in_place_type<derived1>, 42};
		any obj2{std::in_place_type<derived2>, 12.5};
		any obj3{std::in_place_type<derived2>, 124.5};

		CHECK_EQ(obj->f(), 42.0);
		CHECK_EQ(obj2->f(), 12.5);
		CHECK_EQ(obj3->f(), 124.5);

		any valueluess{std::move(obj3)};
		make_valueluess(valueluess);

		CHECK_FALSE(obj3.valueless_by_exception());
		CHECK_EQ(obj3->f(), INFINITY);

		obj2 = std::move(obj);
		CHECK_EQ(obj->f(), INFINITY);
		CHECK_EQ(obj2->f(), 42.0);

		obj = std::move(valueluess);
		CHECK(obj.valueless_by_exception());
		CHECK(valueluess.valueless_by_exception());
		CHECK_THROWS((void) *obj);
		CHECK_THROWS((void) *valueluess);

		valueluess = std::move(obj2);
		CHECK_FALSE(valueluess.valueless_by_exception());
		CHECK_EQ(valueluess->f(), 42.0);
	}
}

TEST_SUITE("inplace_polymophic: copy and move") {
	struct base {
		virtual ~base() = default;

		virtual void copy_to(base *dst) const = 0;
		virtual void move_to(base *dst) = 0;
	};

	struct derived1 : base {
		void copy_to(base *dst) const override {
			new (dst) derived1{*this};
		}

		void move_to(base *dst) override {
			new (dst) derived1{std::move(*this)};
		}
	};

	struct derived2 : base {
		void copy_to(base *dst) const override {
			new (dst) derived2{*this};
		}

		void move_to(base *dst) override {
			new (dst) derived2{std::move(*this)};
		}
	};

	using any = dtl::inplace_polymorphic<base, derived1, derived2>;

	static_assert(std::is_copy_constructible_v<any>);
	static_assert(std::is_copy_assignable_v<any>);
	static_assert(std::is_move_constructible_v<any>);
	static_assert(std::is_move_assignable_v<any>);
}
