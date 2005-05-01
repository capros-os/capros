#ifndef __STACKTESTER_HXX__
#define __STACKTESTER_HXX__
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

#ifdef TEST_STACK

extern "C" {
  extern uint32_t *InterruptStackTop;
}

struct StackTester {
  enum { height = 10 };
  
  uint32_t sum;

  StackTester()
  {
    uint32_t *pstack = &sum;
    sum = 0;
    for (int i = 0; i < height; i++)
      sum += *pstack++;
  }

  void check()
  {
    uint32_t newsum = 0;
    uint32_t *pstack = &sum;
    for (int i = 0; i < height; i++)
      sum += *pstack++;

    assertex(this, sum == newsum);
  }
};

#endif

#endif /* __STACKTESTER_HXX__ */
