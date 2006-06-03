#ifndef __EROS_SETJMP_I486_H__
#define __EROS_SETJMP_I486_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#if !(defined(i386) || defined(i486))
#error "Wrong target file included"
#endif

/* Support for kernel setjmp/longjmp: */
typedef struct
{
  long int bx, si, di;
  kva_t bp;
  kva_t sp;
  kva_t pc;
} jmp_buf[1];

#ifdef __cplusplus
extern "C" {
#endif
  /* Implemented in machine-dependent files!! */
  int setjmp(jmp_buf);
  void longjmp(jmp_buf, int value) NORETURN;
#ifdef __cplusplus
}
#endif

#endif /* __EROS_SETJMP_I486_H__ */

