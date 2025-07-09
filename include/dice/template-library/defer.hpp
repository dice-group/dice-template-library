#ifndef DICE_TEMPLATE_LIBRARY_DEFER_HPP
#define DICE_TEMPLATE_LIBRARY_DEFER_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <exception>

namespace dice::template_library {
	/**
	 * The policy deciding if the scope exit function should be executed
	 */
	enum struct ScopeExitPolicy : uint8_t {
		Always,    ///< Always execute
		OnFail,    ///< Only execute if the scope failed (i.e. an exception was thrown)
		OnSuccess, ///< Only execute if the scope did not fail (i.e. no exception was thrown)
	};

	/**
	 * A RAII type that executes a function on scope exit.
	 * Inspired by Andrei Alexandrescu's “Declarative Control Flow" presentation
	 *
	 * @tparam Pol Policy deciding if the function should be executed
	 * @tparam F type of function to execute
	 */
	template<ScopeExitPolicy Pol, typename F>
	struct ScopeExitGuard {
	private:
		std::optional<F> func_;
		int const uncaught_exceptions_ = std::uncaught_exceptions();

	public:
		ScopeExitGuard() noexcept = default;

		explicit ScopeExitGuard(F &&func) noexcept(std::is_nothrow_move_constructible_v<F>) : func_{std::move(func)} {
		}

		explicit ScopeExitGuard(F const &func) noexcept(std::is_nothrow_copy_constructible_v<F>) : func_{std::move(func)} {
		}

		ScopeExitGuard(ScopeExitGuard const &) = delete;
		ScopeExitGuard &operator=(ScopeExitGuard const &) = delete;
		ScopeExitGuard(ScopeExitGuard &&) = delete;
		ScopeExitGuard &operator=(ScopeExitGuard &&) = delete;

		~ScopeExitGuard() noexcept(Pol != ScopeExitPolicy::OnSuccess) {
			if (!func_.has_value()) {
				return;
			}

			/**
			 * Determines if the scope failed by examining the number of in-flight exceptions.
			 * The baseline number is captured at construction time and is now compared to the current number.
			 * If the number of in-flight exceptions increased, this means the scope must have failed.
			 * Otherwise it must not have failed.
			 */
			[[maybe_unused]] auto const scope_failed = [this]() noexcept {
				return uncaught_exceptions_ < std::uncaught_exceptions();
			};

			if constexpr (Pol == ScopeExitPolicy::OnSuccess) {
				if (!scope_failed()) {
					std::invoke(*func_);
				}
			} else if constexpr (Pol == ScopeExitPolicy::OnFail) {
				if (scope_failed()) {
					std::invoke(*func_);
				}
			} else /* Pol == ScopeExitPolicy::Always */ {
				std::invoke(*func_);
			}
		}
	};

	/**
	 * Constructs a ScopeExitGuard with ScopeExitPolicy::Always and the given function
	 * @param func function to execute on scope exit
	 * @return constructed ScopeExitGuard
	 */
	template<typename F>
	auto make_scope_exit_guard(F &&func) {
		return ScopeExitGuard<ScopeExitPolicy::Always, std::remove_cvref_t<F>>{std::forward<F>(func)};
	}

	/**
	 * Constructs a ScopeExitGuard with ScopeExitPolicy::OnFail and the given function
	 * @param func function to execute on scope exit
	 * @return constructed ScopeExitGuard
	 */
	template<typename F>
	auto make_scope_fail_guard(F &&func) {
		return ScopeExitGuard<ScopeExitPolicy::OnFail, std::remove_cvref_t<F>>{std::forward<F>(func)};
	}

	/**
	 * Constructs a ScopeExitGuard with ScopeExitPolicy::OnSuccess and the given function
	 * @param func function to execute on scope exit
	 * @return constructed ScopeExitGuard
	 */
	template<typename F>
	auto make_scope_success_guard(F &&func) {
		return ScopeExitGuard<ScopeExitPolicy::OnSuccess, std::remove_cvref_t<F>>{std::forward<F>(func)};
	}

	/**
	 * Helper types and functions whose only purpose it to improve
	 * the syntax of the provided `DEFER*` macros to make them seem more like actual language
	 * constructs rather than macros (see macro definitions below).
	 */
	namespace defer_detail {
		struct ScopeGuardOnExit {};
		struct ScopeGuardOnFail {};
		struct ScopeGuardOnSuccess {};

		template<typename F>
		auto operator+(ScopeGuardOnExit, F &&func) {
			return make_scope_exit_guard(std::forward<F>(func));
		}

		template<typename F>
		auto operator+(ScopeGuardOnFail, F &&func) {
			return make_scope_fail_guard(std::forward<F>(func));
		}

		template<typename F>
		auto operator+(ScopeGuardOnSuccess, F &&func) {
			return make_scope_success_guard(std::forward<F>(func));
		}
	} // namespace defer_detail

} // namespace dice::template_library

#define DICE_TEMPLATE_LIBRARY_DETAIL_CONCAT_IMPL(a, b) a ## b
#define DICE_TEMPLATE_LIBRARY_DETAIL_CONCAT(a, b) DICE_TEMPLATE_LIBRARY_DETAIL_CONCAT_IMPL(a, b)
#define DICE_TEMPLATE_LIBRARY_DETAIL_SCOPEGUARD_VAR DICE_TEMPLATE_LIBRARY_DETAIL_CONCAT(_scope_guard, __LINE__)

/**
 * Execute the given expression on scope exit.
 * Note the evaluated expression is not allowed to throw.
 * Naming is inspired by GO's defer and C23's defer proposal
 *
 * Example:
 * @code
 * FILE *f = fopen("path", "r");
 * DICE_DEFER { fclose(f); };
 *
 * // do stuff with f
 * // f closed at end of scope
 * @endcode
 */
#define DICE_DEFER \
	auto DICE_TEMPLATE_LIBRARY_DETAIL_SCOPEGUARD_VAR = ::dice::template_library::defer_detail::ScopeGuardOnExit{} + [&]() noexcept

/**
 * Similar to DICE_DEFER, but it only executes the expression if the scope failed (i.e. the scope threw an exception).
 * Note the evaluated expression is not allowed to throw.
 */
#define DICE_DEFER_TO_FAIL \
	auto DICE_TEMPLATE_LIBRARY_DETAIL_SCOPEGUARD_VAR = ::dice::template_library::defer_detail::ScopeGuardOnFail{} + [&]() noexcept

/**
 * Similar to DICE_DEFER, but it only executes the expression if the scope succeeded (i.e. the scope did not throw an exception).
 * Note the evaluated expression is allowed to throw.
 */
#define DICE_DEFER_TO_SUCCESS \
	auto DICE_TEMPLATE_LIBRARY_DETAIL_SCOPEGUARD_VAR = ::dice::template_library::defer_detail::ScopeGuardOnSuccess{} + [&]()

#endif // DICE_TEMPLATE_LIBRARY_DEFER_HPP
