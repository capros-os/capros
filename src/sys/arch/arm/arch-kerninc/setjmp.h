#ifndef __EROS_SETJMP_ARM_H__
#define __EROS_SETJMP_ARM_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

#if !defined(EROS_TARGET_arm)
#error "Wrong target file included"
#endif

/* Support for kernel setjmp/longjmp: */
typedef struct
{
  uint32_t r4tor11[10];
  uint32_t sp, pc;
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

#endif /* __EROS_SETJMP_ARM_H__ */

