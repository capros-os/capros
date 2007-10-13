#ifndef _ASM_GENERIC_BITOPS_FLS_H_
#define _ASM_GENERIC_BITOPS_FLS_H_

/**
 * fls32 - find last (most-significant) bit set
 * @x: the word to search
 *
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */

static inline unsigned int fls32(uint32_t x)
{
  unsigned int r;

  if (!x)
    return 0;
  if (!(x & 0xffff0000u)) {
    x <<= 16;
    r = 16;
  }
  else r = 32;
  if (!(x & 0xff000000u)) {
    x <<= 8;
    r -= 8;
  }
  if (!(x & 0xf0000000u)) {
    x <<= 4;
    r -= 4;
  }
  if (!(x & 0xc0000000u)) {
    x <<= 2;
    r -= 2;
  }
  if (!(x & 0x80000000u)) {
    r -= 1;
  }
  return r;
}

static inline unsigned int fls64(uint64_t x)
{
  uint32_t h = x >> 32;
  if (h)
    return fls(h) + 32;
  return fls(x);
}

#endif /* _ASM_GENERIC_BITOPS_FLS_H_ */
