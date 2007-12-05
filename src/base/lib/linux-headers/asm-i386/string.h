#ifndef _I386_STRING_H_
#define _I386_STRING_H_

#ifdef __KERNEL__
/*
 * On a 486 or Pentium, we are better off not using the
 * byte string operations. But on a 386 or a PPro the
 * byte string ops are faster than doing it by hand
 * (MUCH faster on a Pentium).
 */

/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strsep,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		NO Copyright (C) 1991, 1992 Linus Torvalds,
 *		consider these trivial functions to be PD.
 */

/* AK: in fact I bet it would be better to move this stuff all out of line.
 */

// Don't use static inline functions because they conflict with
// non-static declarations in <string.h>.

#define __memcpy(t, f, n) memcpy(t, f, n)

#endif /* __KERNEL__ */

#endif
