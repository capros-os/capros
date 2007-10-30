/*
 * Copyright (C) 2001 Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Implementation of the key cache logic. The idea of the key cache is
   to multiplex a potentially large number of keys (stored in a tree)
   onto a smaller number of key registers. If the key cache is in use,
   it takes over slots 12..27 (inclusive) for cached use.

*/

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>

typedef uint32_t keyaddr_t;	/* max size of key space */
typedef uint32_t keyreg_t;	/* max size of key space */

#define FIRST_CACHE_REG 8
#define NREGS 16		/* number used for cacheing! */
#define NBUCKETS EROS_NODE_SIZE

static uint32_t used = ~0u;
static uint32_t age = 0;

/* placeholders: */
void keycache_move_to_slot(keyreg_t slot, keyaddr_t k)
{
}

void keycache_move_to_cache(keyreg_t slot, keyaddr_t k)
{
}

static unsigned 
ffs(uint32_t u)
{
  unsigned i = 1;
  static uint32_t pos[16] = {
    0,				/* 0000 */
    1,				/* 0001 */
    2,				/* 0010 */
    2,				/* 0011 */
    3,				/* 0100 */
    3,				/* 0101 */
    3,				/* 0110 */
    3,				/* 0111 */
    4,				/* 1000 */
    4,				/* 1001 */
    4,				/* 1010 */
    4,				/* 1011 */
    4,				/* 1100 */
    4,				/* 1101 */
    4,				/* 1110 */
    4,				/* 1111 */
  };

  if (u == 0)
    return u;

  if ((u & 0xffffu) == 0) {
    u >>= 16;
    i += 16;
  }
  u &= 0xffffu;

  if ((u & 0xffu) == 0) {
    u >>= 8;
    i += 8;
  }
  u &= 0xffu;

  if ((u & 0xfu) == 0) {
    u >>= 4;
    i += 4;
  }
  u &= 0xfu;

  return pos[u] + i;

}

/* keycache_lru_use() -- if a register is multiply used in a given
   invocation, this is not a good indicator that it is going to remain
   live a long time (quite the contrary, usually), so we only want to
   count it once. What we do is mark the fact of use in the used
   bitmask, and then apply the used info later to update the LRU
   counters.

   The ageing strategy used here is an approximate LRU as follows: if
   the key hasn't been used in three invocations its reclaimable. This
   is accomplished as follows:

   Every key has a 2 bit field. When the key slot is used, this field
   is zeroed. Each time it goes *unused*, it is incremented. When it
   reaches '3', it is a candidate for reclamation. Some care is taken
   to guard against overflow.
*/
void
keycache_lru_use(keyreg_t slot)
{
  slot -= FIRST_CACHE_REG;
  slot *= 2;

  /* if used, clear both bits of the bit pair for the corresponding
     reg. */
  used &= !(3u << slot);
}

keyreg_t
keycache_lru_alloc()
{
  /* Goal: find an entry whose bit pair currently holds '11' but whose
     corresponding bit pair in the used field is not set. Here is how:

     Mask OUT the lower bit of each pair, leaving the upper bit, which
     could be either 0 or 1 (we don't know yet). Each pair now
     contains 'x0'

     Subtract the low bit from each pair. The pair now holds '10' if the
     upper bit was set or '00' if the upper bit was clear.
  */

  uint32_t candidates = 0;
  do {
    /* Anybody with the high bit set is a candidate: */

    candidates = age & 0xaaaaaaaau;

    /* Remove from the candidate list anything that is being used in
       the current invocation: */
    candidates &= used;

    /* Only apply ageing if we found nothing. Exempt registers being
       used in the current invocation from ageing. */
    if (candidates == 0) {
      uint32_t increment = 0x55555555u;
      increment &= used;
      age += increment;
    }
  } while (candidates == 0);

  candidates &= used;

  /* Find the first set bit: */
  {
    keyreg_t slot = ffs(candidates) - 1;
    slot /= 2;
    slot += FIRST_CACHE_REG;

    return slot;
  }
}

void
keycache_commit()
{
  age &= used;
  used = ~0u;
}

struct reguse {
  keyaddr_t k;
  struct reguse *next;
} uses[EROS_NODE_SIZE] = {
  {0, 0},
  {1, 0},
  {2, 0},
  {3, 0},
  {4, 0},
  {5, 0},
  {6, 0},
  {7, 0},
  {8, 0},
  {9, 0},
  {10, 0},
  {11, 0},
  {12, 0},
  {13, 0},
  {14, 0},
  {15, 0},
  {16, 0},
  {17, 0},
  {18, 0},
  {19, 0},
  {20, 0},
  {21, 0},
  {22, 0},
  {23, 0},
  {24, 0},
  {25, 0},
  {26, 0},
  {27, 0},
  {28, 0},
  {29, 0},
  {30, 0},
  {31, 0},
};

struct reguse *reghash[NBUCKETS] = {
  0,
  0,
  0,
  0,

  0,
  0,
  0,
  0,

  0,
  0,
  0,
  0,

  &uses[12],
  &uses[13],
  &uses[14],
  &uses[15],

  &uses[16],
  &uses[17],
  &uses[18],
  &uses[19],

  &uses[20],
  &uses[21],
  &uses[22],
  &uses[23],

  &uses[24],
  &uses[25],
  &uses[26],
  &uses[27],

  0,
  0,
  0,
  0,
};

void
keycache_flush(keyreg_t slot)
{
  struct reguse * bucket;
  keyaddr_t k;

  if (slot < FIRST_CACHE_REG)
    return;

  k = uses[slot].k;
  if (k < FIRST_CACHE_REG)
    return;

  keycache_move_to_cache(slot, k);
  
  bucket = reghash[k % NBUCKETS];

  /* We know it is in this bucket somewhere, so there is no need to
     check for running off the end of the bucket before we find it. */
  if (bucket->k == k) {
    reghash[k % NBUCKETS] = bucket->next;
    bucket->next = 0;
    return;
  }

  while (bucket->next->k != k)
    bucket = bucket->next;

  {

    struct reguse *next = bucket->next;

    bucket->next = next->next;
    next->next = 0;
  }
}

keyreg_t
keycache_alloc_slot(keyaddr_t k)
{
  keyreg_t slot = keycache_lru_alloc();
  keycache_flush(slot);
  uses[slot].k = k;
  uses[slot].next = reghash[k % NBUCKETS];
  reghash[k % NBUCKETS] = &uses[slot];

  return slot;
}

keyreg_t
keycache_isload(keyaddr_t k)
{
  struct reguse *bucket = reghash[k % NBUCKETS];
  while (bucket) {
    if (bucket->k == k)
      return bucket - uses;
  }

  return 0;
}

keyreg_t 
keycache_load(keyaddr_t k)
{
  keyreg_t slot;

  if (k < FIRST_CACHE_REG)
    return k;

  slot = keycache_isload(k);
  if (!slot) {
    slot = keycache_alloc_slot(k);
    keycache_move_to_slot(slot, k);
  }

  keycache_lru_use(slot);
  return slot;
}

keyreg_t
keycache_store(keyaddr_t k)
{
  keyreg_t slot;

  if (k < FIRST_CACHE_REG)
    return k;

  slot = keycache_isload(k);
  if (!slot)
    slot = keycache_alloc_slot(k);

  keycache_lru_use(slot);
  return slot;
}

extern fixreg_t __rt_do_RETURN(Message *);
extern fixreg_t __rt_do_NPRETURN(Message *);
extern fixreg_t __rt_do_CALL(Message *);
extern fixreg_t __rt_do_SEND(Message *);

fixreg_t RETURN(Message *in)
{
  Message msg;
  memcpy(&msg, in, sizeof(msg));

  msg.snd_invKey = keycache_load(in->snd_invKey);

  msg.snd_key0 = keycache_load(in->snd_key0);
  msg.snd_key1 = keycache_load(in->snd_key1);
  msg.snd_key2 = keycache_load(in->snd_key2);
  msg.snd_rsmkey = keycache_load(in->snd_rsmkey);

  msg.rcv_key0 = keycache_store(in->rcv_key0);
  msg.rcv_key1 = keycache_store(in->rcv_key1);
  msg.rcv_key2 = keycache_store(in->rcv_key2);
  msg.rcv_rsmkey = keycache_store(in->rcv_rsmkey);

  keycache_commit();

  return __rt_do_RETURN(&msg);
}

fixreg_t NPRETURN(Message *in)
{
  Message msg;
  memcpy(&msg, in, sizeof(msg));

  msg.snd_invKey = keycache_load(in->snd_invKey);

  msg.snd_key0 = keycache_load(in->snd_key0);
  msg.snd_key1 = keycache_load(in->snd_key1);
  msg.snd_key2 = keycache_load(in->snd_key2);
  msg.snd_rsmkey = keycache_load(in->snd_rsmkey);

  msg.rcv_key0 = keycache_store(in->rcv_key0);
  msg.rcv_key1 = keycache_store(in->rcv_key1);
  msg.rcv_key2 = keycache_store(in->rcv_key2);
  msg.rcv_rsmkey = keycache_store(in->rcv_rsmkey);

  keycache_commit();

  return __rt_do_NPRETURN(&msg);
}

fixreg_t CALL(Message *in)
{
  Message msg;

  memcpy(&msg, in, sizeof(msg));

  msg.snd_invKey = keycache_load(in->snd_invKey);

  msg.snd_key0 = keycache_load(in->snd_key0);
  msg.snd_key1 = keycache_load(in->snd_key1);
  msg.snd_key2 = keycache_load(in->snd_key2);
  msg.snd_rsmkey = keycache_load(in->snd_rsmkey);

  msg.rcv_key0 = keycache_store(in->rcv_key0);
  msg.rcv_key1 = keycache_store(in->rcv_key1);
  msg.rcv_key2 = keycache_store(in->rcv_key2);
  msg.rcv_rsmkey = keycache_store(in->rcv_rsmkey);

  keycache_commit();

  return __rt_do_CALL(&msg);
}

void SEND(Message *in)
{
  Message msg;
  memcpy(&msg, in, sizeof(msg));

  msg.snd_invKey = keycache_load(in->snd_invKey);

  msg.snd_key0 = keycache_load(in->snd_key0);
  msg.snd_key1 = keycache_load(in->snd_key1);
  msg.snd_key2 = keycache_load(in->snd_key2);
  msg.snd_rsmkey = keycache_load(in->snd_rsmkey);

  /* No keys will be received, so don't even bother. */

  keycache_commit();

  __rt_do_SEND(&msg);
}
