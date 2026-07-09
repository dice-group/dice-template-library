#ifndef DICE_TEMPLATELIBRARY_SANDBOX_HPP
#define  DICE_TEMPLATELIBRARY_SANDBOX_HPP

#include <dice/template-library/type_traits.hpp>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace dice::template_library {

    /**
     * Execution result of a subprocess
     */
    enum struct SubProcessResult : int {
        ExitSuccess = -1,  ///< normal exit, code == 0
        ExitFailure = -2,  ///< normal exit, code != 0
        Interrupted = SIGINT,
        IllegalInstruction = SIGILL,
        Aborted = SIGABRT,
        FloatingPointException = SIGFPE,
        SegmentationFault = SIGSEGV,
        Terminated = SIGTERM,
    };

    namespace detail_sandbox {
        inline void clear_all_signal_handlers() {
            static constexpr int max_sig = [] {
#ifdef NSIG
                return NSIG;
#elifdef _NSIG
                return _NSIG;
#else
                return 64; // Fallback
#endif
            }();

            sigset_t empty_sigs;
            sigemptyset(&empty_sigs);

            // unblock global, inhibited signals
            ::sigprocmask(SIG_SETMASK, &empty_sigs, nullptr);

            struct sigaction sa{};
            sa.sa_handler = SIG_DFL;
            sa.sa_mask = empty_sigs; // unblock local, inhibited signals
            sa.sa_flags = 0;

            for (int i = 1; i < max_sig; ++i) {
                ::sigaction(i, &sa, nullptr); // ignore errors (like EINVAL for unknown signals)
            }
        }

        inline void flush_all_streams() {
            // normally the fflush below also handles
            // these but if std::ios_base::sync_with_stdio(false);
            // was ever called they have their own buffers
            std::cout.flush();
            std::cerr.flush();

            fflush(nullptr);

            // OS file descriptors do not need to be flushed
        }

        // the noexcept is important: if func throws, this turns it into std::terminate signalling SIGABRT
        template<typename F>
            requires (is_strict_invocable_r_v<void, F> || is_strict_invocable_r_v<int, F>)
        [[nodiscard]] int invoke_like_main(F &&func) noexcept {
            if constexpr (std::is_same_v<std::invoke_result_t<F>, void>) {
                std::invoke(std::forward<F>(func));
                return 0;
            } else {
                return std::invoke(std::forward<F>(func));
            }
        }

        struct SandBox {};

        template<typename F>
        [[nodiscard]] SubProcessResult operator+(SandBox, F func) {
            static_assert(std::is_invocable_r_v<void, F> || std::is_invocable_r_v<int, F>,
                          "Function must be invocable like a main() function");

            // ensure no stale data is in output streams
            flush_all_streams();

            int const pid = fork();
            if (pid < 0) {
                throw std::system_error{errno, std::system_category(), "Unable to fork"};
            }

            if (pid == 0) {
                // child process

                // remove signal handles that may have been installed (e.g. by doctest)
                clear_all_signal_handlers();

                // invoke user provided function

                int const exit_code = invoke_like_main(std::move(func));

                // flush again, to avoid loosing output if the function does not abort
                flush_all_streams();

                // ensure no destructors can run even if func does not abort
                _exit(exit_code);
            }

            // parent process

            int wstatus;
            int res;
            do {
                res = ::waitpid(pid, &wstatus, 0);
            } while (res < 0 && errno == EINTR);

            if (res < 0) {
                throw std::system_error{errno, std::system_category(), "waitpid failed"};
            }

            if (WIFEXITED(wstatus)) {
                if (WEXITSTATUS(wstatus) == 0) {
                    return SubProcessResult::ExitSuccess;
                } else {
                    return SubProcessResult::ExitFailure;
                }
            } else if (WIFSIGNALED(wstatus)) {
                return static_cast<SubProcessResult>(WTERMSIG(wstatus));
            } else {
                // this should be unreachable with the flags passed to waitpid
                throw std::runtime_error{"Process exited in a weird way"};
            }
        }
    } // namespace detail_sandbox
} // namespace dice::template_library

/**
 * Run the provided code in a separate process.
 * This can, for example, be used to ensure that a particular piece of code triggers an assertion.
 *
 * @return enum describing how the subprocess exited
 * @note exceptions are turned into SubProcessResult::Aborted
 *
 * @pre The application is single threaded when this is called,
 *      or at least the other threads are not holding any global locks.
 *
 * @pre The provided function must not use std::exit to exit the subprocess, use return instead.
 *      Ignoring this precondition causes destructors to run and potentially close resources the parent still needs.
 *
 * @example
 * @code
 * auto const res = DICE_SANDBOX {
 *     assert(false);
 * };
 *
 * assert(res == SubProcessResult::Aborted);
 * @endcode
 */
#define DICE_SANDBOX ::dice::template_library::detail_sandbox::SandBox{} + [&]()

#endif //  DICE_TEMPLATELIBRARY_SANDBOX_HPP
