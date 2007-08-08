/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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

/* Directory object.

   Maintains a mapping from names to keys.  Key operations are LINK,
   LOOKUP, UNLINK.  Uses the BSD-style namespace manipulation routines
   to manage the mapping. A key difference between this code and the
   BSD code is that it does not concern itself with the underlying
   object size.  Instead, we rely on consistent checkpointing for our
   recoverability.

   NOTE that parts of this code are subject to the BSD copyright,
   which is appended at the bottom of the file.

   FIX: An open issue with this design is whether it shouldn't use
   UNICODE internally.  This can be done transparently preserving the
   existing interface by adding new order codes, but I'm sorely
   tempted to do it now.

   I assume that the instruction to be executed is in rcv_code
   and the data in rcv_data.
   The key or the error code is returned in snd_key1 */


#include <stdio.h>
#include <eros/target.h>
#include <domain/Runtime.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Node.h>

#include <domain/SuperNodeKey.h>
#include <domain/ConstructorKey.h>
#include <domain/DirectoryKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <stdlib.h>
#include <string.h>
#include "constituents.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

#define KR_SNODE       KR_APP(0)
#define KR_OSTREAM     KR_APP(1)
#define KR_SCRATCH     KR_APP(2)
#define KR_ARG0        KR_ARG(0)

#define dbg_init    0x1
#define dbg_op      0x2
#define dbg_link    0x4
#define dbg_unlink  0x8
#define dbg_lookup  0x10
#define dbg_find    0x20
#define dbg_freemap 0x40

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)




/* Theoretically, directories can be more than 1Gb in length, however,
 * in practice this seems unlikely, and we don't currently support
 * it. */

#define	doff_t		Word
#define	MAXDIRSIZE	0x4000000 /* 1 GB */

/* A directory consists of some number of directory entry structures,
 * which are of variable length.  Each directory entry has a struct
 * direct at the front of it, containing its entry number, the length
 * of the entry, and the length of the name contained in the entry.
 * These are followed by the name padded to a 4 byte boundary with
 * null bytes.  All names are guaranteed null terminated.  The maximum
 * length of a name in a directory is MAXNAMLEN.
 *
 * The macro DIRSIZ(fmt, dp) gives the amount of space required to
 * represent a directory entry.  Free space in a directory is
 * represented by entries which have dp->d_reclen > DIRSIZ(fmt, dp).
 *
 * Entries are not permitted to span page boundaries; the last entry
 * in a block will incorporate the free space at the end of the page
 * in it's dp->d_reclen field.
 *
 * When entries are deleted from a directory, the space is returned by
 * simply marking the dp->dt_inuse field false.  When entries are
 * added to a directory, a page with sufficient space is first
 * located.  If one exists, it's free space is first coalesced and
 * then the new entry is added at the end.  If none exists, a new page
 * is tacked on to the end of the directory to hold the insertion.
 * The (empty) space AFTER the last entry is named by state->dir_top.
 *
 * The idea is not to mutate more than one page at a time.  If
 * coalescence were allowed to span page boundaries, the write time
 * would be substantially larger. */

#define	MAXNAMLEN	255

struct	direct {
  uint32_t d_entno;	/* directory ndx */
  uint16_t d_reclen;	/* length of this record */
  uint8_t  d_inuse;	/* entry has content or not */
  uint8_t  d_namlen;	/* true length of string in d_name */
  char	   d_name[MAXNAMLEN + 1];/* name with length <= MAXNAMLEN */
};

/* The DIRSIZ macro gives the minimum record length which will hold
 * the directory entry.  This requires the amount of space in struct
 * direct without the d_name field, plus enough space for the name
 * with a terminating null byte (dp->d_namlen+1), rounded up to a 4
 * byte boundary. */

#define DIRSIZ(dp) ((sizeof(struct direct) - (MAXNAMLEN+1)) + (((dp)->d_namlen+1 + 3) &~ 3))

#define NEXTDIR(dp) ((struct direct *) ((uint8_t *)dp + dp->d_reclen))

#define PAGESTART(dp) ((struct direct *) ((uint32_t)dp & (~(EROS_PAGE_MASK))))


const struct dir_template {
  uint32_t	   dot_entno;
  uint16_t d_reclen;
  uint8_t	   dot_inuse;
  uint8_t	   dot_namlen;
  char	   dot_name[4];	/* must be multiple of 4 */

  uint32_t	   dotdot_entno;
  uint16_t dotdot_reclen;
  uint8_t	   dotdot_inuse;
  uint8_t	   dotdot_namlen;
  char	   dotdot_name[4]; /* ditto */
} template = {
  0,				/* slot 0 */
  12,				/* reclen for "." entry */
  true,				/* entry in use */
  1,				/* name length */
  { '.', 0, 0, 0 },		/* null-padded "." */
  1,				/* slot 1 */
  EROS_PAGE_SIZE - 12, 		/* reclen for ".." entry */
  true,				/* entry in use */
  2,				/* name length */
  { '.', '.', 0, 0 }		/* null-padded ".." */
};

const struct new_page_template {
  uint32_t	   dot_entno;
  uint16_t d_reclen;
  uint8_t	   dot_inuse;
  uint8_t	   dot_namlen;
  char	   dot_name[4];	/* must be multiple of 4 */
} blank_page = {
  0,				/* N/A */
  EROS_PAGE_SIZE,		/* to end of page */
  false,			/* entry in use */
  0,				/* name length */
  { 0, 0, 0, 0 }		/* placeholder, not used! */
};

#define N_FREEMAP_PAGE 0x1

typedef struct {
  uint32_t ndirent;			/* number of entries in directory */
  struct direct *dir_top;
  uint32_t freeMap[(EROS_PAGE_SIZE * N_FREEMAP_PAGE)/sizeof(uint32_t)];
  char name[MAXNAMLEN+1];	/* incoming name on requests + NULL */
} state_t;

#if CONVERSION
typedef uint32_t bool;
#endif

const uint32_t __rt_stack_pages = 0x1 + N_FREEMAP_PAGE;

/* First dirent is at the address of the VCS */
#define VCS_LOCATION 0x40000000u

struct direct * const first_entry = (struct direct *) VCS_LOCATION;

static unsigned int
BlssToL2v(unsigned int blss)
{
  // assert(blss > 0);
  return (blss -1 - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS;
}

uint32_t
alloc_dirent(state_t *state)
{
  uint32_t w;
  uint32_t max_w = EROS_PAGE_SIZE * N_FREEMAP_PAGE / sizeof(uint32_t);
  
  DEBUG(freemap) kprintf(KR_OSTREAM, "state->freeMap is 0x%08x\n", state->freeMap);

  for (w = 0; w < max_w; w++) {
    if (state->freeMap[w] != UINT32_MAX) {
      /* at least one bit is free. */
      uint32_t bit;

      DEBUG(freemap) kprintf(KR_OSTREAM, "Word %d is 0x%08x\n", w, state->freeMap[w]);
      for (bit = 0; bit < UINT32_BITS; bit++) {
	uint32_t which_bit = bit ? (1 << bit) : 1;
	if ((state->freeMap[w] & which_bit) == 0) {
	  DEBUG(freemap) kdprintf(KR_OSTREAM, "Setting bit %d of word %d\n",
		   bit, w);
	  state->freeMap[w] |= which_bit;

	  return bit + w * UINT32_BITS;
	}
      }
    }
  }

  return 0;
}

void
free_dirent(state_t *state, uint32_t ndx)
{
  uint32_t w = ndx / UINT32_BITS;
  uint32_t bit = ndx % UINT32_BITS;
  uint32_t which_bit = bit ? (1 << bit) : 1;
  
  state->freeMap[w] &= ~which_bit;
}

struct direct *
find(char *name, state_t * state)
{
  struct direct *dp = first_entry;
  size_t len = strlen(name);

  DEBUG(find) kdprintf(KR_OSTREAM, "find(\"%s\" [%d]): top=0x%08x\n",
		       name, len, state->dir_top);
	     
  while (dp != state->dir_top) {
    DEBUG(find) kdprintf(KR_OSTREAM, "find(\"%s\"): dp=0x%08x len %d ent %d (\"%s\")\n",
	     name, dp, dp->d_namlen, dp->d_entno,
	     dp->d_inuse ? dp->d_name : "<not in use>");

    if (dp->d_inuse &&
	dp->d_namlen == len &&
	strcmp(dp->d_name, name) == 0)
      return dp;
    
    dp = NEXTDIR(dp);
  }
  
  return 0;
}

uint32_t
lookup (char *name, uint32_t kr, state_t *state)
{
  uint32_t result;
  
  struct direct *dp = find(name, state);

  if (!dp)
    return RC_Directory_NotFound;
  
  DEBUG(lookup) kdprintf(KR_OSTREAM, "lookup(\"%s\"): call to snode w/ entry %d\n",
	   name, dp->d_entno);

  if ((result = supernode_copy(KR_SNODE, dp->d_entno, kr)) != RC_OK) {
    kdprintf(KR_OSTREAM, "Result from supernode_copy: 0x%08x\n", result);
    return result;
  }

  return RC_OK;

}

uint32_t
unlink(char *name, uint32_t kr, state_t * state)
{
  struct direct *dp = find(name, state);

  if (dp) {
    supernode_swap(KR_SNODE, dp->d_entno, KR_VOID, KR_SCRATCH);
    dp->d_inuse = false;
    free_dirent(state, dp->d_entno);

    /* file_unreference(KR_SCRATCH); */
    return RC_OK;
  }
  else
    return RC_Directory_NotFound;
}

/* The passed dirent contains sufficient space. */
bool
insert_dirent(struct direct *dp, char *name, uint32_t kr, state_t *state)
{
  uint32_t entno;
  uint32_t len = strlen(name);

  uint32_t padlen = len;
  padlen += 4;			/* round up to NEXT 4 byte multiple */
  padlen &= ~3u;
  padlen += 8;			/* overhead per dirent */
  
#ifdef NDEBUG
  if (dp->d_inuse) {
    DEBUG(link) kdprintf(KR_OSTREAM, "insert_dirent(): dp 0x%08x in use already!\n",
	     dp);
  }
#endif
  
  entno = alloc_dirent(state);
#if 0
  if (entno == 0)
    return false;
#endif

  
  DEBUG(link) kdprintf(KR_OSTREAM, "insrt_de(0x%08x, \"%s\", %d) entno %d\n",
	   dp, name, kr, entno);
  
  dp->d_entno = entno;
  /* no change to dp->d_reclen */
  dp->d_inuse = true;
  dp->d_namlen = len;
  bcopy(name, dp->d_name, len);

  while (padlen > len)		/* zero-pad the name */
    dp->d_name[len++] = 0;

  supernode_swap(KR_SNODE, entno, kr, KR_VOID);

  return true;
}

/* This routine is only called by link(), and only when it is already
   known that there exists sufficient space in the page to make use
   of. It walks through the entries in the page, bringing all of the
   in-use entries to the front of the page. Leave an entry with
   d_inuse == false at the end containing all the free space. */
struct direct *
coalesce_page(struct direct *dp)
{
  uint32_t offset = 0;
  uint32_t bytes = 0;
  uint32_t free = 0;
  
  uint8_t *page_start = (uint8_t *)dp;
  
#ifndef NDEBUG
  if ((uint32_t) dp & EROS_PAGE_MASK)
    DEBUG(link) kdprintf(KR_OSTREAM, "coalesce(): start dp 0x%08x not at page boundary\n", dp);
#endif

  while (bytes < EROS_PAGE_SIZE) {
    if (dp->d_inuse == false) {
      free += dp->d_inuse;
    }
    else {
      uint32_t truelen = DIRSIZ(dp);
      uint32_t dp_free = dp->d_reclen - DIRSIZ(dp);
      
      if (offset != bytes) {
	bcopy(dp, page_start + offset, truelen);
	offset += truelen;
      }

      ((struct direct *) (page_start + offset))->d_reclen = truelen;
      
      free += dp_free;
    }
      
    bytes += dp->d_reclen;
  }

  if (free < 12)
    DEBUG(link) kdprintf(KR_OSTREAM, "Insufficient free space in page\n");
  
  /* All active entries have now been moved to the front of the page,
     and FREE now contains the number of free bytes in this page.
     Create a new, unallocated struct direct at the end of the page
     which contains all the free space. */

  {
    struct direct *tail;
    tail = (struct direct *) (page_start + free);
    tail->d_reclen = free;
    tail->d_inuse = false;
    tail->d_namlen = 0;
    tail->d_name[0] = 0;
    tail->d_name[1] = 0;
    tail->d_name[2] = 0;
    tail->d_name[3] = 0;

    return tail;
  }
}

struct direct *
find_space(uint32_t len, state_t *state)
{
  struct direct *dp = first_entry;

  DEBUG(link) kdprintf(KR_OSTREAM, "find_space(%d)\n", len);

  while (dp != state->dir_top) {
    struct direct * page_start_dp = dp;

    uint32_t bytes = 0;
    uint32_t free_bytes = 0;

    while (bytes < EROS_PAGE_SIZE) {
      uint32_t free = dp->d_reclen - DIRSIZ(dp);

      DEBUG(link)
	kdprintf(KR_OSTREAM, "Considering dp=0x%08x sz %d reclen %d inuse? %c\n",
		 dp, DIRSIZ(dp), dp->d_reclen, dp->d_inuse ? 'y' : 'n');
    
      if (dp->d_inuse == false)
	free += DIRSIZ(dp);
      
      if (dp->d_inuse == false && free >= len)
	return dp;

      if (dp->d_inuse && free >= len) {
	/* Fashion a new entry here: */
	dp->d_reclen = DIRSIZ(dp);

	dp = NEXTDIR(dp);
	dp->d_reclen = free;
	dp->d_inuse = false;
	dp->d_namlen = 0;
	dp->d_name[0] = 0;
	dp->d_name[1] = 0;
	dp->d_name[2] = 0;
	dp->d_name[3] = 0;

	return dp;
      }
	
      free_bytes += free;
      bytes += dp->d_reclen;
      dp = NEXTDIR(dp);
    }

    if (len <= free_bytes) {
      dp = coalesce_page(page_start_dp);
      return dp;
    }
  }

  return 0;
}

uint32_t
link(char *name, uint32_t kr, state_t * state)
{
  uint32_t len;
  struct direct *dp = find(name, state);

  if (dp)
    return RC_Directory_Exists;

  /* This is NOT the standard roundup.  if the name is exactly a
     multiple of 4 bytes, we WANT the trailing 4 bytes to get added: */
  len = strlen(name);
  len += 4;			/* round up to NEXT 4 byte multiple */
  len &= ~3u;
  len += 8;			/* overhead per dirent */
  
  /* Run through the directory looking for a page that has sufficient
     space.  The dp->d_namlen test works when we overrun the current
     directory because the VCS will demand allocate a zero page for
     us. */

  dp = first_entry;

  if ( (dp = find_space(len, state)) == 0 ) {
    dp = state->dir_top;
    bcopy(&blank_page, state->dir_top, sizeof(blank_page));
    state->dir_top =
      (struct direct *) ((uint8_t *)state->dir_top + EROS_PAGE_SIZE);
  }

  DEBUG(link) kdprintf(KR_OSTREAM, "Inserting dirent at dp=0x%08x\n", dp);
  if ( insert_dirent(dp, name, kr, state) == false )
    return RC_Directory_NoSpace;
  
  return RC_OK;
}

int
ProcessRequest(Message *msg, state_t *state)
{
  switch (msg->rcv_code) {
    
    
  case OC_Directory_Lookup:
    DEBUG(op) kprintf(KR_OSTREAM, "DIR: lookup(\"%s\")\n", state->name);
    msg->snd_code = lookup (state->name, KR_ARG0, state);
    msg->snd_key0 = KR_ARG0;
    break;

  case OC_Directory_Link:
    DEBUG(op) kprintf(KR_OSTREAM, "DIR: link(\"%s\", <key>)\n", state->name);
    msg->snd_code = link (state->name, msg->rcv_key0, state);
    break;

  case OC_Directory_Unlink:
    DEBUG(op) kprintf(KR_OSTREAM, "DIR: unlink(\"%s\") => <key>\n", state->name);
    msg->snd_code = unlink (state->name, msg->rcv_key0, state);
    break;

  case OC_capros_key_getType:			/* check alleged keytype */
    msg->snd_code = RC_OK;
    msg->snd_w1 = AKT_Directory;
    break;

  default:
    msg->snd_code = RC_capros_key_UnknownRequest;
    break;
  };  
  
  return 1;
}

void
Initialize(state_t *state)
{
  uint32_t result;
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  /* Using KR_ARG0, KR_SNODE as scratch registers */

  /* Construct the ZS that will hold the actual directory data */

  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: Building ZS\n");

  capros_Node_getSlot(KR_CONSTIT, KC_ZSF, KR_SNODE);
  result = constructor_request(KR_SNODE, KR_BANK, KR_SCHED, KR_VOID, KR_SNODE);

  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: Result is: 0x%08x\n", result);

  /* Buy the new root node to hold it: */
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_ARG0);
  if (result != RC_OK)
    DEBUG(init) kdprintf(KR_OSTREAM, "DIR: spcbank GPTs exhausted\n", result);
  
  /* make that GPT LSS=TOP_LSS */
  capros_GPT_setL2v(KR_ARG0, BlssToL2v(EROS_ADDRESS_BLSS));

  /* plug in newly allocated ZSF */
  DEBUG(init) kdprintf(KR_OSTREAM, 
		       "DIR: plugging zsf into new spc root\n", result);
  capros_GPT_setSlot(KR_ARG0, 8, KR_SNODE);

  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: fetch my own space\n", result);
  process_copy(KR_SELF, ProcAddrSpace, KR_SNODE);

  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: plug self spc into new spc root\n", result);
  capros_GPT_setSlot(KR_ARG0, 0, KR_SNODE);
  
  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: before lobotomy\n", result);
  process_swap(KR_SELF, ProcAddrSpace, KR_ARG0, KR_VOID);
  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: post lobotomy\n", result);

  capros_Node_getSlot(KR_CONSTIT, KC_SNODEC, KR_SNODE);
  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: buying supernode\n");
  result = constructor_request(KR_SNODE, KR_BANK, KR_SCHED, KR_VOID, KR_SNODE);
  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: Got it. Result 0x%08x\n", result);

  /* insert key for ".." entry: */
  supernode_swap(KR_SNODE, 1, KR_ARG0, KR_VOID);

  /* make start key to us: */
  process_make_start_key(KR_SELF, 0, KR_ARG0);
  
  /* insert key for "." entry: */
  supernode_swap(KR_SNODE, 0, KR_ARG0, KR_VOID);

  state->ndirent = 2;
  {	/* the compiler doesn't seem to be able to do this in one statement. */
    char * foo = (char *)first_entry;
    foo = foo + EROS_PAGE_SIZE;
    state->dir_top = (struct direct *) foo;
  }
  bzero(state->freeMap, EROS_PAGE_SIZE * N_FREEMAP_PAGE);

  bcopy(&template, first_entry, sizeof(template));
	
  DEBUG(init) kdprintf(KR_OSTREAM, "Allocate Initial slots...\n");

  (void) alloc_dirent(state);
  (void) alloc_dirent(state);

  DEBUG(init) kdprintf(KR_OSTREAM, "Initial slots allocated\n");
}

int
main()
{
  state_t state;
  Message msg;

  Initialize(&state);

  process_make_start_key(KR_SELF, 0, KR_ARG0);


  DEBUG(init) kdprintf(KR_OSTREAM, "DIR: Got start key. Ready to rock and roll\n");
  kdprintf(KR_OSTREAM, "DIR: Got start key. Ready to rock and roll\n");
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_ARG0;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_ARG0;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = state.name;
  msg.rcv_limit = MAXNAMLEN;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    fixreg_t got = 0;

    RETURN(&msg);

    got = min(msg.rcv_limit, msg.rcv_sent);
    ((uint8_t *) msg.rcv_data)[got] = 0;

    msg.snd_key0 = KR_VOID;		 /* until otherwise proven */
    kdprintf(KR_OSTREAM, "Before ProcessRequest(): freemap = 0x%08x\n",
	     state.freeMap);
  } while ( ProcessRequest(&msg, &state) );

  return 0;
}


/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)dir.h	8.4 (Berkeley) 8/10/94
 */
