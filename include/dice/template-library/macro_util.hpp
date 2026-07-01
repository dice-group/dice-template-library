#ifndef DICE_TEMPLATELIBRARY_MACROUTIL_HPP
#define DICE_TEMPLATELIBRARY_MACROUTIL_HPP

#ifdef __has_builtin
/**
 * Detect if the compiler has the given builtin.
 * @param builtin the builtin to check for
 */
#define DICE_COMPILER_HAS_BUILTIN(builtin) __has_builtin(builtin)
#else // __has_builtin
/**
 * DICE_COMPILER_HAS_BUILTIN is not supported on the current compiler and
 * will always return false.
 */
#define DICE_COMPILER_HAS_BUILTIN(builtin) 0
#endif // __has_builtin


#define DICE_IMPL_IDENT_CONCAT(a, b) a ## b

/**
 * Concatenate two identifiers.
 *
 * @param a first identifier
 * @param b second identifier
 */
#define DICE_IDENT_CONCAT(a, b) DICE_IMPL_IDENT_CONCAT(a, b)


#ifdef __FILE_NAME__
/**
 * The name of the current file.
 */
#define DICE_FILENAME __FILE_NAME__
#else // __FILE_NAME__
/**
 * The name of the current file.
 * Fallback implementation. Uses __FILE__ instead of the unavailable __FILE_NAME__.
 */
#define DICE_FILENAME __FILE__
#endif // __FILE_NAME__


#if defined(__has_attribute) && __has_attribute(weak)
#define DICE_WEAK __attribute__((weak))
#define DICE_HAS_WEAK 1
#else
#define DICE_WEAK
#define DICE_HAS_WEAK 0
#endif

#if DICE_HAS_WEAK
// forward declaration for __lsan_ignore_object from <sanitizer/lsan_interface.h> as a weak symbol.
// If the sanitizer is linked this it set to the correct value by the linker, if not this is set to nullptr by the linker.
extern "C" DICE_WEAK void __lsan_ignore_object(void const *ptr); // NOLINT(bugprone-reserved-identifier)
#endif

namespace dice::template_library {
    /**
     * Tell the leak sanitizer to ignore that the given object is leaked.
     */
    inline void ignore_leak(void const *ptr) noexcept {
#if DICE_HAS_WEAK
        if (__lsan_ignore_object) {
            __lsan_ignore_object(ptr);
        }
#endif

        (void) ptr;
    }

    namespace detail_ignore_leak {
        [[deprecated("DICE_IGNORE_LEAK is deprecated. Use dice::template_library::ignore_leak() instead.")]]
        inline void deprecated_macro_use() {
        }
    }  // namespace detail_ignore_leak
}  // namespace dice::template_library

/**
 * Tell the leak sanitizer to ignore that the given object is leaked.
 * This macro is deprecated, use dice::template_library::ignore_leak instead.
 */
#define DICE_IGNORE_LEAK(ptr) (::dice::template_library::detail_ignore_leak::deprecated_macro_use(), ::dice::template_library::ignore_leak(ptr))

#endif // DICE_TEMPLATELIBRARY_MACROUTIL_HPP
