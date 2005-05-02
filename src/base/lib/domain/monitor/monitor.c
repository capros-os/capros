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

#include <eros/target.h>
#include <eros/machine/atomic.h>
#include <eros/Invoke.h>
#include <domain/ConstructorKey.h>

#include <domain/monitor.h>

/* Monitor library.  Provides 32 efficient mutual exclusion monitors.
   The basic idea is to set and clear these monitors with atomic
   in-memory operations whenever possible, falling back to a
   coordination domain only when the in-memory mechanism fails.  The
   general idea is to thereby provide a reasonably high-concurrency
   mutex. */

/* mon_init(pWord, locks): initialize a heavy monitor and place a
   capability to it in register /kr/ */
void
mon_init(Monitor *pMon, uint32_t krMutex)
{
  pMon->krMutex = krMutex;
  pMon->wantCount = 0;
  pMon->locks = 0;
}

static void
invoke_heavy_lock(Monitor *pMon, int val)
{
  Message m;
  m.snd_code = val;
  m.snd_w1 = 0;
  m.snd_w2 = 0;
  m.snd_w3 = 0;
  m.snd_key0 = 0;
  m.snd_key1 = 0;
  m.snd_key2 = 0;
  m.snd_rsmkey = 0;
  m.snd_data = 0;
  m.rcv_code = RC_OK;
  m.rcv_w1 = 0;
  m.rcv_w2 = 0;
  m.rcv_w3 = 0;
  m.rcv_key0 = 0;
  m.rcv_key1 = 0;
  m.rcv_key2 = 0;
  m.rcv_rsmkey = 0;
  m.rcv_data = 0;
  m.snd_invKey = pMon->krMutex;
  CALL(&m);
}

/* mon_lock(pWord, locks): grab the lock bits specified in /locks/,
   setting the corresponding bits in /pWord/ as a side effect.  If
   possible, do so using in-memory operations.  Otherwise, fall back
   to the concurrency manager to coordinate. */
void
mon_lock(Monitor *pMon, uint32_t locks)
{
  ATOMIC_INC32(&pMon->wantCount);

  for(;;) {
    uint32_t old_locks = pMon->locks;
    uint32_t new_locks = old_locks | locks;
  
    /* if any of what we want is locked when we check, it's no good. */
    old_locks &= ~locks;
  
    if(ATOMIC_SWAP32(&pMon->locks, old_locks, new_locks) == 1)
      break;
    
    invoke_heavy_lock(pMon, 1);
  }

  ATOMIC_DEC32(&pMon->wantCount);
}

/* mon_unlock(pWord, locks): release the lock bits specified in
   /locks/, setting the corresponding bits in /pWord/ as a side
   effect.  If possible, do so using in-memory operations.  Otherwise,
   fall back to the concurrency manager to coordinate. */
void
mon_unlock(Monitor *pMon, uint32_t locks)
{
  for(;;) {
    uint32_t old_locks = pMon->locks;
    uint32_t new_locks = old_locks & ~locks;
  
    if(ATOMIC_SWAP32(&pMon->locks, old_locks, new_locks) == 1)
      break;
  }

  if (pMon->wantCount)
    invoke_heavy_lock(pMon, -1);
}
