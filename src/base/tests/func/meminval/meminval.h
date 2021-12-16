/*
 * Copyright (C) 2007, Strawberry Development Group
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

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

#define KR_OSTREAM 10
#define KR_PAGE  12
#define KR_SEG17 13
#define KR_SEG22 14
#define KR_SEG27 15
#define KR_SEG32 16
#define KR_PROC1_PROCESS 17
#define KR_TEMP 27

int __assert(const char *expr, const char *file, int line)
{
  kprintf(KR_OSTREAM, "%s:%d: Assertion failed: '%s'\n",
           file, line, expr);
  return 0;
}

#define assert(expression)  \
  ((void) ((expression) ? 0 : __assert (#expression, __FILE__, __LINE__)))

struct sharedInfo {
  int blss;
  uint32_t faultCode;
  uint32_t faultInfo;
};
struct sharedInfo * shInfP = (struct sharedInfo *) ADDR1;

