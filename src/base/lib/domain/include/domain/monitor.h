/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
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

typedef struct Monitor {
  uint32_t krMutex;
  uint32_t wantCount;
  uint32_t locks;
} Monitor;

/* mon_init(pWord, locks): initialize a heavy monitor and place a
   capability to it in register /kr/ */
extern void
mon_init(Monitor *pMon, uint32_t krMutex);

/* mon_lock(pWord, locks): grab the lock bits specified in /locks/,
   setting the corresponding bits in /pWord/ as a side effect.  If
   possible, do so using in-memory operations.  Otherwise, fall back
   to the concurrency manager to coordinate. */
void
mon_lock(Monitor *pMon, uint32_t locks);

/* mon_unlock(pWord, locks): release the lock bits specified in
   /locks/, setting the corresponding bits in /pWord/ as a side
   effect.  If possible, do so using in-memory operations.  Otherwise,
   fall back to the concurrency manager to coordinate. */
void
mon_unlock(Monitor *pMon, uint32_t locks);

