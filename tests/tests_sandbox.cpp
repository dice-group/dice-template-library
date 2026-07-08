#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "dice/template-library/defer.hpp"

#include <doctest/doctest.h>

#include <dice/template-library/sandbox.hpp>

#include <csignal>
#include <cstdio>
#include <fstream>
#include <string>

#include <fcntl.h>
#include <unistd.h>

TEST_SUITE("DICE_SANDBOX") {
    using namespace dice::template_library;

    TEST_CASE("success") {
        SUBCASE("fallthrough") {
            auto const res = DICE_SANDBOX {};

            CHECK_EQ(res, SubProcessResult::ExitSuccess);
        }
        SUBCASE("explicit 0") {
            auto const res = DICE_SANDBOX {
                return 0;
            };

            CHECK_EQ(res, SubProcessResult::ExitSuccess);
        }
    }

    TEST_CASE("normal exit failure") {
        SUBCASE("return") {
            auto const res = DICE_SANDBOX {
                return 42;
            };

            CHECK_EQ(res, SubProcessResult::ExitFailure);
        }
    }

    TEST_CASE("exception") {
        auto const res = DICE_SANDBOX {
            throw std::runtime_error{"Hello"};
        };

        CHECK_EQ(res, SubProcessResult::Aborted);
    }

    TEST_CASE("std::abort") {
        auto const res = DICE_SANDBOX {
            std::abort();
        };

        CHECK_EQ(res, SubProcessResult::Aborted);
    }

    TEST_CASE("object sharing") {
        std::vector<int> x{};

        auto const res = DICE_SANDBOX {
            x.push_back(12);
        };

        CHECK_EQ(res, SubProcessResult::ExitSuccess);
        CHECK(x.empty());
    }

    TEST_CASE("stdout sharing") {
        std::cout << "Hello "; // no flush

        auto const res = DICE_SANDBOX {
            std::cout << "World";
        };

        std::cout.flush();

        CHECK_EQ(res, SubProcessResult::ExitSuccess);
    }

    TEST_CASE("stderr sharing") {
        std::cerr << "Hello "; // no flush

        auto const res = DICE_SANDBOX {
            std::cerr << "World";
        };

        std::cerr.flush();

        CHECK_EQ(res, SubProcessResult::ExitSuccess);
    }

    TEST_CASE("signal mapping") {
        SUBCASE("SIGINT") {
            auto const res = DICE_SANDBOX {
                ::raise(SIGINT);
            };
            CHECK_EQ(res, SubProcessResult::Interrupted);
        }
        SUBCASE("SIGILL") {
            auto const res = DICE_SANDBOX {
                ::raise(SIGILL);
            };
            CHECK_EQ(res, SubProcessResult::IllegalInstruction);
        }
        SUBCASE("SIGFPE") {
            auto const res = DICE_SANDBOX {
                ::raise(SIGFPE);
            };
            CHECK_EQ(res, SubProcessResult::FloatingPointException);
        }
        SUBCASE("SIGSEGV") {
            auto const res = DICE_SANDBOX {
                ::raise(SIGSEGV);
            };
            CHECK_EQ(res, SubProcessResult::SegmentationFault);
        }
        SUBCASE("SIGTERM") {
            auto const res = DICE_SANDBOX {
                ::raise(SIGTERM);
            };
            CHECK_EQ(res, SubProcessResult::Terminated);
        }
        SUBCASE("SIGABRT") {
            auto const res = DICE_SANDBOX {
                ::raise(SIGABRT);
            };
            CHECK_EQ(res, SubProcessResult::Aborted);
        }
    }

    TEST_CASE("real segfault") {
        auto const res = DICE_SANDBOX {
            // force real segfault
            *static_cast<int volatile *>(nullptr) = 42;
        };

        CHECK_EQ(res, SubProcessResult::SegmentationFault);
    }

    TEST_CASE("value capture") {
        int const y = 42;

        auto const res = DICE_SANDBOX {
            return y - 42; // uses the [=]-captured y
        };

        CHECK_EQ(res, SubProcessResult::ExitSuccess);
    }

    TEST_CASE("file descriptor side effects propagate") {
        // in contrast to "object sharing": memory is copy-on-write, but effects
        // on shared OS file descriptors are visible to the parent
        char path[] = "/tmp/dice_sandbox_XXXXXX";
        int const fd = ::mkstemp(path);
        REQUIRE(fd >= 0);

        DICE_DEFER {
            ::close(fd);
            ::unlink(path);
        };

        auto const res = DICE_SANDBOX {
            ::write(fd, "hello", 5);
        };

        CHECK_EQ(res, SubProcessResult::ExitSuccess);

        ::lseek(fd, 0, SEEK_SET);
        char buf[6] = {};
        auto const n = ::read(fd, buf, 5);
        CHECK_EQ(n, 5);
        CHECK_EQ(std::string_view{buf}, "hello");
    }
}
