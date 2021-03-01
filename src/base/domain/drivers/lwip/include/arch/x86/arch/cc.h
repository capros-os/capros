#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__
/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Define platform endianness */
#define BYTE_ORDER LITTLE_ENDIAN

typedef unsigned char u8_t;
typedef signed char s8_t;
typedef unsigned short u16_t;
typedef short s16_t;
typedef unsigned long u32_t;
typedef long s32_t;
typedef u32_t mem_ptr_t;

/* Define (sn)printf formatters for these lwIP types */
#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"

/* Compiler hints for packing structures */
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PROVIDE_ERRNO

/* Plaform specific diagnostic output */
#include <domain/cmte.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
asmlinkage int printk(const char * fmt, ...);
#define LWIP_PLATFORM_DIAG(x) do {printk x;} while(0)

#define LWIP_PLATFORM_ASSERT(x) \
  kdprintf(KR_OSTREAM, "%s:%d: failed assertion `" x "'\n", \
           __FILE__, __LINE__ )

/* The library has no htons etc, so we must provide them: */
static inline u16_t lwip_swab16(u16_t x) {
  return x << 8 | x >> 8;
}
static inline u32_t lwip_swab32(u32_t x) {
  return x<<24 | x>>24 | (x & 0x0000ff00UL)<<8 | (x & 0x00ff0000UL)>>8;
}
#define LWIP_PLATFORM_BYTESWAP 1
#define LWIP_PLATFORM_HTONS(x) lwip_swab16(x)
#define LWIP_PLATFORM_HTONL(x) lwip_swab32(x)

#endif /* __ARCH_CC_H__ */
