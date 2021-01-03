#ifndef __LINUX_COMPILER_H
#error "Please don't include <linux/compiler-gcc10.h> directly, include <linux/compiler.h> instead."
#endif

/* I guess we'll either update or cull these headers at some point.
 *
 * These are a best guess at what they might look like if linux 2
 * had support for gcc 8 and 10.
 *
 * - WL
 */

#define __used			__attribute__((__used__))
#define __must_check 		__attribute__((warn_unused_result))
#define __compiler_offsetof(a,b) __builtin_offsetof(a,b)
#define __always_inline		inline __attribute__((always_inline))

/*
 * A trick to suppress uninitialized variable warning without generating any
 * code
 */
#define uninitialized_var(x) x = x

#define __cold			__attribute__((__cold__))
