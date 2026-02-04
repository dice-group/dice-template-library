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

#endif // DICE_TEMPLATELIBRARY_MACROUTIL_HPP
