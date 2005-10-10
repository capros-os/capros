#ifndef __KERNEL_H__
#define __KERNEL_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#include <eros/target.h>
#include <disk/ErosTypes.h>
     
#define DOWNCAST(child, parent) (parent *) (child)

/* These could be inline functions, but this way it works for C as
   well: */
#define min(a,b) ((a) <= (b) ? (a) : (b))
#define max(a,b) ((a) >= (b) ? (a) : (b))

/* Support for the assert macro: */

#ifndef OPTION_DDB
INLINE void Debugger() {}
#else
extern void Debugger();
#endif

/* fatal() and printf() are implemented in kernel/kern_printf.c */
#ifdef OPTION_DDB
extern void fatal(const char* msg, ...);
#else
extern void fatal(const char* msg, ...) NORETURN;
#endif

extern void dprintf(unsigned shouldStop, const char* msg, ...);
extern void printf(const char* msg, ...);
extern void printOid(OID oid);
extern void printCount(ObCount count);

#ifdef NDEBUG

#define assert(ignore) ((void) 0)
#define assertex(ptr, ignore) ((void) 0)
#define REQUIRE(ignore) ((void) 0)

#else

extern int __assert(const char *, const char *, int);
extern int __assertex(const void *ptr, const char *, const char *, int);
extern void __require(const char *, const char *, int);

#define assert(expression)  \
  ((void) ((expression) ? 0 : __assert (#expression, __FILE__, __LINE__)))
#define assertex(ptr, expression)  \
  ((void) ((expression) ? 0 : __assertex (ptr, #expression, __FILE__, __LINE__)))
#define REQUIRE(expression)  \
  do { if ( ! (expression)  ) { __require (#expression, __FILE__, __LINE__); ckresult = false; } } while (0)
#endif

#ifdef offsetof
#undef offsetof
#endif
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#if defined(DBG_WILD_PTR)
extern unsigned dbg_wild_ptr;
#endif
#ifndef NDEBUG
extern unsigned dbg_inttrap;
#endif    

void heap_init();
void *malloc(size_t);
#define MALLOC(type,count) ((type *) malloc(sizeof(type) * count))

#endif /* __KERNEL_H__ */
