#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/macro_util.hpp>
#include <dice/template-library/sandbox.hpp>

#include <array>
#include <memory>

#define MY_IDENT world

// true if the ASan runtime is linked in
bool asan_active() noexcept {
#if DICE_HAS_WEAK
	return __asan_poison_memory_region != nullptr;
#else
	return false;
#endif
}

// ASan reports an access to poisoned memory by terminating the process
void check_poisoned_access(dice::template_library::SubProcessResult res) {
	if (asan_active()) {
		CHECK_NE(res, dice::template_library::SubProcessResult::ExitSuccess);
	} else {
		CHECK_EQ(res, dice::template_library::SubProcessResult::ExitSuccess);
	}
}

// read that cannot be optimized away
std::byte read_byte(std::byte const &b) noexcept {
	return static_cast<std::byte const volatile &>(b);
}

TEST_SUITE("macro_util") {
	TEST_CASE("concat idents") {
		int DICE_IDENT_CONCAT(hello_, world) = 42;
		CHECK_EQ(hello_world, 42);

		DICE_IDENT_CONCAT(hello_, MY_IDENT) = 12;
		CHECK_EQ(hello_world, 12);
	}

    TEST_CASE("filename") {
	    std::string_view const f = DICE_FILENAME;
	    CHECK(f.ends_with("tests_macro_util.cpp"));
	}

    TEST_CASE("ignore leak") {
	    auto *my_obj = new int{42};
	    dice::template_library::ignore_leak(my_obj);
	}

    TEST_CASE("ignore leak, deprecated macro") {
	    auto *my_obj = new int{42};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	    DICE_IGNORE_LEAK(my_obj);
#pragma GCC diagnostic pop
	}

	TEST_CASE("poison memory region") {
		using namespace dice::template_library;

		// heap-allocated and 8-byte aligned, so that ASan shadow granules map exactly
		auto buf = std::make_unique<std::array<std::byte, 64>>();

		SUBCASE("poisoned region is inaccessible") {
			check_poisoned_access(DICE_SANDBOX {
				poison_memory_region(buf->data() + 16, 32);
				read_byte((*buf)[16]);
			});

			check_poisoned_access(DICE_SANDBOX {
				poison_memory_region(buf->data() + 16, 32);
				read_byte((*buf)[47]);
			});
		}

		SUBCASE("outside of poisoned region stays accessible") {
			auto const res = DICE_SANDBOX {
				poison_memory_region(buf->data() + 16, 32);
				read_byte((*buf)[15]);
				read_byte((*buf)[48]);
			};
			CHECK_EQ(res, SubProcessResult::ExitSuccess);
		}

		SUBCASE("unpoison makes region accessible again") {
			auto const res = DICE_SANDBOX {
				poison_memory_region(buf->data() + 16, 32);
				unpoison_memory_region(buf->data() + 16, 32);
				read_byte((*buf)[16]);
				read_byte((*buf)[47]);
			};
			CHECK_EQ(res, SubProcessResult::ExitSuccess);
		}

		SUBCASE("zero size is a no-op") {
			auto const res = DICE_SANDBOX {
				poison_memory_region(buf->data(), 0);
				unpoison_memory_region(nullptr, 0);
				read_byte((*buf)[0]);
			};
			CHECK_EQ(res, SubProcessResult::ExitSuccess);
		}
	}
}