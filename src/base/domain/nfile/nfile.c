/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2009, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */


/*
 * file.c
 *
 * This is the server-based implementation.  In this implementation,
 * a file is actually an object maintained by a server.  The purpose
 * of the server is to amortize the cost of the receive buffer across
 * a whole bunch of files.
 *
 * There is a strictly temporary limitation in the current
 * implementation that the total size of the served space must not
 * exceed 134Mbytes.  This is because the current kernel omits the
 * background window capability implementation, so we are temporarily
 * unable to do windowing tricks.
 *
 * The whole shebang is designed around 4Kbyte file blocks.
 */

#include <stddef.h>
#include <limits.h>
#include <eros/target.h>
#include <domain/Runtime.h>
#include <domain/assert.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/HTTPResource.h>
#include <idl/capros/File.h>
#include <idl/capros/FileServer.h>
#include <idl/capros/Forwarder.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <string.h>
#include <stdlib.h>

#include "constituents.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

#define dbg_init    0x1
#define dbg_alloc   0x2
#define dbg_findpg  0x4
#define dbg_write   0x8
#define dbg_read    0x10
#define dbg_ino     0x20
#define dbg_inogrow 0x40
#define dbg_req     0x80
#define dbg_free    0x100
#define dbg_fresh   0x200

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0x0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_CURFILE    KR_APP(1)
#define KR_OSTREAM    KR_APP(2)
#define KR_MYSPACE    KR_APP(3)
#define KR_SCRATCH    KR_APP(4)

#define BUF_SZ  capros_key_messageLimit
#define BLOCK_SIZE 4096
#define PtrsPerBlock (BLOCK_SIZE/sizeof(uint32_t *))
#define logPtrsPerBlock 10	// compute this manually, sigh

#define NSTACK (BUF_SZ/EROS_PAGE_SIZE + 1)
const uint32_t __rt_stack_pages = NSTACK;

#define GROW_NOZERO 2
#define GROW        1
#define NO_GROW     0

typedef capros_File_fileLocation f_size_t;
#define PS_FSIZE "%#llx"	// printf specification for f_size_t

/* We divide the CAPROS_FAST_SPACE_LGSIZE space into two halves: */
#define SUBSPACE_LGSIZE (CAPROS_FAST_SPACE_LGSIZE-1)
/* The first subspace is for program, bss, and stack.
The second is for file storage, which grows upwards. */

#define INO_NINDIR 11

/* A file is defined by an inode aka struct ino.
Inodes are allocated in the root file.
Each inode has an inode ID which fits in 15 bits
so it, with a read-only bit, can be used in the keyInfo field of a start key.
(An alternative design would set the inode address as the dataWord
of the forwarder, but then snd_w3 is not available for parameters
from the client.)
 */
typedef uint16_t inoID_t;
#define keyInfo_server 0x7fff
#define keyInfo_readOnly 0x8000

/* Following structure is of a size that evenly divides
   BLOCK_SIZE, which is important to the implementation
   (not sure why - CRL) */
typedef struct ino ino_s;
struct ino {
  union {
    uint64_t sz;
    ino_s    *nxt_free;
  } u;
  uint64_t uuid;		/* unique ID */
  uint8_t  nLayer;	// number of levels of indirection blocks
  inoID_t id;		// this inode's own id

  /* A level 0 pointer is a pointer to a block of file data.
     A level n pointer (n > 0) is a pointer to an indirection block
       consisting of an array of level (n-1) pointers. 
     A level n pointer (n >= 0) is NULL if there is no data at that location.
     indir[i] is a level nLayer pointer. */
  uint32_t *indir[INO_NINDIR];
} ;

#define inodesPerBlock (BLOCK_SIZE / sizeof(ino_s))

/* Max of 5 indirection blocks */
const uint64_t sizes_by_layers[] = {
  1llu * INO_NINDIR * BLOCK_SIZE,
  1llu * INO_NINDIR * BLOCK_SIZE
    * PtrsPerBlock,
  1llu * INO_NINDIR * BLOCK_SIZE 
    * PtrsPerBlock
    * PtrsPerBlock,
  1llu * INO_NINDIR * BLOCK_SIZE 
    * PtrsPerBlock
    * PtrsPerBlock
    * PtrsPerBlock,
  1llu * INO_NINDIR * BLOCK_SIZE 
    * PtrsPerBlock
    * PtrsPerBlock
    * PtrsPerBlock
    * PtrsPerBlock,
};


typedef struct server_state {
  uint8_t *buf;

  ino_s    *first_free_inode;
  uint32_t *first_free_block;
  ino_s    root;
  uint8_t * top_addr;	// highest allocated addr +1
  uint64_t nxt_uuid;
} server_state;

#define WANT_ZERO 1
#define NO_ZERO   0
uint8_t*
AllocBlock(server_state *ss, int wantZero)
{
  uint8_t * pg;
  
  if (ss->first_free_block) {
    uint32_t *nxt = (uint32_t *) *(ss->first_free_block);
    pg = (uint8_t *) ss->first_free_block;
    ss->first_free_block = nxt;

    if (wantZero == WANT_ZERO)
      bzero(pg, BLOCK_SIZE);
  }
  else {
    if (ss->top_addr >= (uint8_t *) (2ul << SUBSPACE_LGSIZE))
      kdprintf(KR_OSTREAM, "Nfile: out of address space!!\n");

    pg = ss->top_addr;
    ss->top_addr += BLOCK_SIZE;
    /* Newly allocated pages come to us pre-zeroed by VCSK. */

    DEBUG(fresh)
      kprintf(KR_OSTREAM, "Allocating fresh page\n");
  }

  DEBUG(alloc)
    kdprintf(KR_OSTREAM, "AllocBlock returns 0x%x\n", pg);
  return pg;
}

void
init(server_state *ss)
{
  uint32_t result;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  capros_Process_getAddrSpace(KR_SELF, KR_SCRATCH);

  /* Buy a new root GPT: */
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_MYSPACE);
  if (result != RC_OK)
    DEBUG(init) kdprintf(KR_OSTREAM, "DIR: Spacebank nodes exhausted 0x%x\n",
                         result);
  
  DEBUG(init)
    kdprintf(KR_OSTREAM, "Bought new space\n");

  result = capros_GPT_setL2v(KR_MYSPACE, SUBSPACE_LGSIZE);

  /* Install old space in slot 0. */
  capros_GPT_setSlot(KR_MYSPACE, 0, KR_SCRATCH);

  capros_Process_swapAddrSpace(KR_SELF, KR_MYSPACE, KR_VOID);
  
  capros_Node_getSlot(KR_CONSTIT, KC_ZSF, KR_SCRATCH);
  result = constructor_request(KR_SCRATCH, KR_BANK, KR_SCHED,
			       KR_VOID, KR_SCRATCH);
  assert(result == RC_OK);

  /* plug in newly allocated ZSF */
  DEBUG(init) kdprintf(KR_OSTREAM, 
		       "FS: plugging zsf into new spc root\n", result);
  capros_GPT_setSlot(KR_MYSPACE, 1, KR_SCRATCH);

  bzero(ss, sizeof(*ss));
  ss->top_addr = (uint8_t *) (1ul << SUBSPACE_LGSIZE);
  ss->first_free_inode = 0;
  ss->first_free_block = 0;
  ss->root.nLayer = 0;
  ss->root.uuid = 0;
  ss->nxt_uuid = 1;

  {
    uint32_t sz = BLOCK_SIZE;
    
    ss->buf = AllocBlock(ss, WANT_ZERO);
    while (sz < BUF_SZ) {
      AllocBlock(ss, WANT_ZERO);
      sz += BLOCK_SIZE;
    }

    /* Ensure the pages have been manifested by VCSK, so they can
    be used to receive a message string. */
    for (sz = 0; sz < BUF_SZ; sz += EROS_PAGE_SIZE) {
      // kdprintf(KR_OSTREAM, "Nfile: touching 0x%08x\n", &ss->buf[sz]);
      ss->buf[sz] = 0;
    }
  }
  
  DEBUG(init)
    kdprintf(KR_OSTREAM, "Root inode is initialized\n");
}

/* find_file_page(): Given a *byte* position named by /at/, returns
 * the address of the INDIRECTION TABLE ENTRY for the page.  Thus:
 *
 *     *(find_file_page(...)) = new_page_ptr
 *
 * or
 *     new_page_ptr = *(find_file_page(...))
 *
 * This function does lazy file expansion!
 *
 * Returns NULL if the position is past the end and NO_GROW was specified.
 */
uint32_t **
find_file_page(server_state *ss, ino_s *ino, f_size_t at, int wantGrow)
{
  uint64_t atPg = (at / BLOCK_SIZE);

  // Allocated size is the size including the block containing at.
  uint64_t allocSz = (atPg + 1) * BLOCK_SIZE;
  uint32_t ndx;
  uint32_t **blockTable = ino->indir;
    
  DEBUG(findpg)
    kdprintf(KR_OSTREAM, "find: ino: %#llx sz %#llx at: "PS_FSIZE", grow? %c\n",
	    ino->uuid, ino->u.sz, at, (wantGrow == GROW) ? 'y' : 'n');
  
  /* Grow the file upwards as necessary. */
  while (allocSz > sizes_by_layers[ino->nLayer]) {
    int i;
    uint32_t **newIndir;
      
    DEBUG(findpg)
      kdprintf(KR_OSTREAM, "find: ino: %#llx allocSz %#llx nLayer %d (max %#llx) => grow\n",
	      ino->uuid, allocSz, ino->nLayer,
	      sizes_by_layers[ino->nLayer]);
  
    newIndir = (uint32_t **) AllocBlock(ss, WANT_ZERO);

    for (i = 0; i < INO_NINDIR; i++) {
      newIndir[i] = ino->indir[i];
      ino->indir[i] = 0;
    }
    ino->indir[0] = (uint32_t*) newIndir;
    ino->nLayer++;
  }

  DEBUG(findpg)
    kdprintf(KR_OSTREAM, "find: ino: %#llx nLayer: %d\n",
	    ino->uuid, ino->nLayer);
  
  {
    uint32_t layer = ino->nLayer;

    while (layer > 0) {
      ndx = atPg >> (layer * logPtrsPerBlock);
      ndx %= PtrsPerBlock;
    
      DEBUG(findpg)
	kdprintf(KR_OSTREAM, "find: ino: %#llx layer %d ndx %d\n",
		ino->uuid, layer, ndx);

      blockTable = (uint32_t **)blockTable[ndx];

      DEBUG(findpg)
	kdprintf(KR_OSTREAM, "find: ino: %#llx layer %d ndx %d ==> 0x%x\n",
		ino->uuid, layer, ndx, blockTable);

      if (blockTable == 0) {
	if (wantGrow == NO_GROW)
	  return 0;

	blockTable[ndx] = (uint32_t *) AllocBlock(ss, WANT_ZERO);
	blockTable = (uint32_t **)blockTable[ndx];

	DEBUG(findpg)
	  kdprintf(KR_OSTREAM, "find: ino: %#llx layer %d ndx %d: grow layer: 0x%x\n",
		  ino->uuid, layer, ndx, blockTable);
      }
    
      layer--;
    }

    ndx = atPg % PtrsPerBlock;

    DEBUG(findpg)
      kdprintf(KR_OSTREAM, "find: ino: %#llx layer %d ndx %d: blockTbl 0x%x bt[ndx] 0x%x\n",
	       ino->uuid, layer, ndx, blockTable, blockTable[ndx]);

    if (blockTable[ndx] == 0 && wantGrow != NO_GROW) {
      DEBUG(findpg)
	kdprintf(KR_OSTREAM, "find: ino: %#llx layer %d ndx %d: grow leaf: 0x%x\n",
		 ino->uuid, layer, ndx, blockTable[ndx]);

      if (wantGrow == GROW)
	blockTable[ndx] = (uint32_t *) AllocBlock(ss, WANT_ZERO);
      else
	blockTable[ndx] = (uint32_t *) AllocBlock(ss, NO_ZERO);
    }
  }
  
  DEBUG(findpg)
    kdprintf(KR_OSTREAM, "find: ino: %#llx return 0x%x contains 0x%x\n",
	    ino->uuid, &blockTable[ndx], blockTable[ndx]);

  return &blockTable[ndx];
}

/* Write /len/ bytes of data from /buf/ into /file/, starting at
   position /at/.  Extends the file as necessary. */
result_t
write_to_file(server_state *ss, ino_s *ino, f_size_t at,
	      uint32_t len, uint8_t *buf)
{
  DEBUG(write)
    kdprintf(KR_OSTREAM, "write: ino: %#llx writing %d at "PS_FSIZE"\n",
	    ino->uuid, len, at);

  /* The passed /buf/ is contiguous, but there is no guarantee that
     the file itself is. */
  while (len) {
    uint32_t offset = at & (BLOCK_SIZE - 1);
    uint32_t nBytes = BLOCK_SIZE - offset;

    if (nBytes > len)
      nBytes = len;

    DEBUG(write)
      kdprintf(KR_OSTREAM, "write: ino: %#llx bwrite %d at "PS_FSIZE"\n",
	       ino->uuid, nBytes, at);

    {
      uint32_t grow =
	(offset == 0 && nBytes == BLOCK_SIZE) ? GROW_NOZERO : GROW;
      
      uint32_t **ppPage = find_file_page(ss, ino, at, grow);
      uint8_t *pPage = (uint8_t *) *ppPage;

      memcpy(&pPage[offset], buf, nBytes);

      DEBUG(write)
	kdprintf(KR_OSTREAM,
                 "write: ino: %#llx bwrote %d at "PS_FSIZE" to %#x %d\n",
		 ino->uuid, nBytes, at, &pPage[offset], buf[0]);

      len -= nBytes;
      at += nBytes;
      buf += nBytes;
    }
  }

  if (at > ino->u.sz)
    ino->u.sz = at;
  
  return RC_OK;
}


/* Write /len/ bytes of data from /buf/ into /file/, starting at
   position /at/.  Extends the file as necessary. */
result_t
read_from_file(server_state *ss, ino_s *ino, f_size_t at,
	       uint32_t *rqLen, uint8_t *buf)
{
  uint32_t len = *rqLen;
  
  DEBUG(read)
    kdprintf(KR_OSTREAM, "read: ino: %#llx reading %d at "PS_FSIZE"\n",
	    ino->uuid, len, at);

  if (at > ino->u.sz)
    return RC_capros_key_RequestError;

  if (at + len > ino->u.sz) {
    len = ino->u.sz - at;	// may be zero
    *rqLen = len;

    DEBUG(read)
      kdprintf(KR_OSTREAM, "read: ino: %#llx truncated to %d at "PS_FSIZE"\n",
	       ino->uuid, len, at);
  }
  
  /* The passed /buf/ is contiguous, but there is no guarantee that
     the file itself is. */
  while (len) {
    uint32_t offset = at & (BLOCK_SIZE - 1);
    uint32_t nBytes = BLOCK_SIZE - offset;

    if (nBytes > len)
      nBytes = len;

    {
      uint32_t **ppPage = find_file_page(ss, ino, at, NO_GROW);

      if (ppPage == 0) {
	DEBUG(read)
	  kdprintf(KR_OSTREAM,
                   "read: ino: %#llx bread %d at "PS_FSIZE" -- lazy zero\n",
		   ino->uuid, nBytes, at);

	bzero(buf, nBytes);
      }
      else {
	uint8_t *pPage = (uint8_t *) *ppPage;

	memcpy(buf, &pPage[offset], nBytes);

        DEBUG(read)
          kdprintf(KR_OSTREAM,
                   "read: ino: %#llx bread %d at "PS_FSIZE" from %#x %d\n",
	           ino->uuid, nBytes, at, &pPage[offset], buf[0]);
      }
    }

    /* blockTable now points to the start of the content page */
    len -= nBytes;
    at += nBytes;
    buf += nBytes;
  }

  return RC_OK;
}

ino_s *
inodeIDToPtr(server_state * ss, inoID_t id)
{
  int blk = id / inodesPerBlock;
  uint32_t ** ppPage = find_file_page(ss, &ss->root, blk * BLOCK_SIZE, NO_GROW);
  ino_s * pIno = (ino_s *) *ppPage;
  return &pIno[id - blk * inodesPerBlock];
}

result_t
grow_inode_table(server_state *ss)
{
  int i;
  
  f_size_t oldRootSize = ss->root.u.sz;

  DEBUG(inogrow)
    kdprintf(KR_OSTREAM, "ino: ss=%#x: growing inode table from "PS_FSIZE"\n",
             ss, oldRootSize);

  f_size_t oldBlocks = oldRootSize / BLOCK_SIZE;
  assert(oldBlocks <= ULONG_MAX);	// because keyInfo_server < ULONG_MAX
  int newNumInodes = ((unsigned long)oldBlocks+1) * inodesPerBlock;
  if (newNumInodes >= keyInfo_server)
    // inode ID won't fit in keyInfo.
    return RC_capros_FileServer_TooManyFiles;
  
  /* Extend the file with a new zero block: */
  uint32_t ** ppPage = find_file_page(ss, &ss->root, oldRootSize, GROW);
  ino_s * pIno = (ino_s *) *ppPage;
  
  DEBUG(inogrow)
    kdprintf(KR_OSTREAM, "ino: pIno=0x%x, %d, %d\n", pIno, BLOCK_SIZE, sizeof(ino_s));

  int idBase = newNumInodes - inodesPerBlock;
  for (i = 0; i < inodesPerBlock; i++) {
    pIno[i].id = idBase + i;	// this inode's own id
    pIno[i].u.nxt_free = &pIno[i+1];
  }

  pIno[i-1].u.nxt_free = 0;

  ss->first_free_inode = &pIno[0];
  ss->root.u.sz += BLOCK_SIZE;
  return RC_OK;
}

result_t
create_new_file(server_state *ss, ino_s** outFile)
{
  ino_s * ino;
  result_t result;
  
  if (ss->first_free_inode == 0) {
    result = grow_inode_table(ss);
    if (result != RC_OK)
      return result;
  }

  /* Now have at least one free inode */

  ino = ss->first_free_inode;
  ss->first_free_inode = ino->u.nxt_free;

  int i;
  for (i = 0; i < INO_NINDIR; i++) {
    assert(ino->indir[i] == 0);
  }
  ino->u.sz = 0;
  ino->nLayer = 0;
  ino->uuid = ss->nxt_uuid;
  ss->nxt_uuid++;
  
  DEBUG(ino)
    kprintf(KR_OSTREAM, "ino: created new file ino=0x%x with uuid %#llx\n",
	     ino, ino->uuid);
  
  *outFile = ino;
  return RC_OK;
}

void
reclaim_ino_pages(server_state *ss, uint32_t lvl, uint32_t *blockTable)
{
  if (blockTable) {
    if (lvl) {
      int i;
      for (i = 0; i < BLOCK_SIZE/sizeof(uint32_t); i++)
        reclaim_ino_pages(ss, lvl - 1, (uint32_t *) blockTable[i]);
    }
  
    DEBUG(free)
      kdprintf(KR_OSTREAM, "Freeing pg 0x%x\n", blockTable);
  
    *blockTable = (uint32_t) ss->first_free_block;
    ss->first_free_block = blockTable;
  }
}

void
destroy_file(server_state *ss, ino_s *ino)
{
  int i;
  
  DEBUG(ino)
    kprintf(KR_OSTREAM, "ino: destroy file ino=0x%x with uuid %#llx\n",
	     ino, ino->uuid);

  for (i = 0; i < INO_NINDIR; i++) {
    reclaim_ino_pages(ss, ino->nLayer, ino->indir[i]);
    ino->indir[i] = 0;
  }

  ino->u.nxt_free = ss->first_free_inode;
  ss->first_free_inode = ino;
}

int
ProcessRequest(Message *msg, server_state *ss)
{
  result_t result = RC_OK;
  
  msg->snd_key0 = KR_VOID;
  msg->snd_len = 0;		/* until proven otherwise */
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;

  if (msg->rcv_keyInfo == keyInfo_server) {
    switch(msg->rcv_code) {
    default:
      result = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      msg->snd_w1 = IKT_capros_FileServer;
      break;

    case OC_capros_key_destroy:
      assert(false);//// incomplete
      break;

    case OC_capros_FileServer_createFile:
    {
      ino_s * ino;

      DEBUG(req)
	kdprintf(KR_OSTREAM, "NFILE: create\n");

      result = create_new_file(ss, &ino);
      if (result != RC_OK)
	break;

      /* Each file has two forwarders; one read-only, one read-write. */
      result = capros_SpaceBank_alloc2(KR_BANK, 
                 capros_Range_otForwarder + (capros_Range_otForwarder << 8),
                 KR_CURFILE, KR_TEMP1);
      if (result != RC_OK) {
        destroy_file(ss, ino);
	break;
      }

      // Set read-write target:
      result = capros_Process_makeStartKey(KR_SELF, ino->id, KR_TEMP0);
      assert(result == RC_OK);
      result = capros_Forwarder_swapTarget(KR_CURFILE, KR_TEMP0, KR_VOID);
      assert(result == RC_OK);

      // Set read-only target:
      result = capros_Process_makeStartKey(KR_SELF,
                 ino->id | keyInfo_readOnly, KR_TEMP0);
      assert(result == RC_OK);
      result = capros_Forwarder_swapTarget(KR_TEMP1, KR_TEMP0, KR_VOID);
      assert(result == RC_OK);

/* Slot fwdr_roSlot of read-write forwarder has
 * non-opaque key to read-only forwarder. */
#define fwdr_roSlot 0
      result = capros_Forwarder_swapSlot(KR_CURFILE, fwdr_roSlot, KR_TEMP1,
                 KR_VOID);
      assert(result == RC_OK);

      result = capros_Forwarder_getOpaqueForwarder(KR_CURFILE,
                 capros_Forwarder_sendCap, KR_TEMP0);
      assert(result == RC_OK);

      msg->snd_key0 = KR_TEMP0;	// return opaque read-write key
      break;
    }
    }
  } else {	// not the server; an individual file
    ino_s * ino = inodeIDToPtr(ss, msg->rcv_keyInfo & 0x7fff);

#define checkRW if (msg->rcv_keyInfo & keyInfo_readOnly) { \
                  result = RC_capros_key_NoAccess; break; }

    switch (msg->rcv_code) {
    default:
      result = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      msg->snd_w1 = IKT_capros_File;
      break;

    case OC_capros_key_destroy:
    {
      checkRW

      DEBUG(req)
	kdprintf(KR_OSTREAM, "NFILE: ino %#llx destroy\n",
		 ino->uuid);

      destroy_file(ss, ino);
  
      // Rescind and free the forwarders:
      capros_Forwarder_getSlot(KR_CURFILE, fwdr_roSlot, KR_TEMP0);
      result_t rez = capros_SpaceBank_free2(KR_BANK, KR_CURFILE, KR_TEMP0);
      if (rez != RC_OK)
        kdprintf(KR_OSTREAM, "NFILE: free(fwdr) returned %#x\n", rez);

      break;
    }

    case 0:	// OC_capros_File_read
    {
      f_size_t at = msg->rcv_w1 | ((uint64_t)msg->rcv_w2 << 32);
      uint32_t len = msg->rcv_w3;
      
      DEBUG(req)
	kdprintf(KR_OSTREAM, "NFILE: ino %#llx read %d at "PS_FSIZE"\n",
		 ino->uuid, len, at);

      result = read_from_file(ss, ino,  at, &len, ss->buf);
      msg->snd_data = ss->buf;
      msg->snd_len = len;
      break;
    }

    case 1:	// OC_capros_File_write
    {
      checkRW

      f_size_t at = msg->rcv_w1 | ((uint64_t)msg->rcv_w2 << 32);
      uint32_t len = msg->rcv_w3;
      
      DEBUG(req)
	kdprintf(KR_OSTREAM, "NFILE: ino %#llx write %d at "PS_FSIZE" %d\n",
		 ino->uuid, len, at, ss->buf[0]);

      result = write_to_file(ss, ino,  at, len, ss->buf);

      msg->snd_w1 = len;
      break;
    }

    case OC_capros_File_getSize:
      msg->snd_w1 = (uint32_t) ino->u.sz;
      msg->snd_w2 = ino->u.sz >> 32;
      break;

    case OC_capros_File_getReadOnlyCap:
    {
      cap_t k;
      if (msg->rcv_keyInfo & keyInfo_readOnly) {
        k = KR_CURFILE;		// non-opaque key to read-only forwarder
      } else {
        capros_Forwarder_getSlot(KR_CURFILE, fwdr_roSlot, KR_TEMP1);
        k = KR_TEMP1;
      }
      result = capros_Forwarder_getOpaqueForwarder(k,
                 capros_Forwarder_sendCap, KR_TEMP0);
      assert(result == RC_OK);

      msg->snd_key0 = KR_TEMP0;	// return opaque read-only key
      break;
    }

    case 2:	// OC_capros_HTTPResource_request
      result = capros_Forwarder_getOpaqueForwarder(KR_CURFILE,
                 capros_Forwarder_sendCap, KR_TEMP0);
      assert(result == RC_OK);

      msg->snd_w1 = capros_HTTPResource_RHType_File;
      msg->snd_key0 = KR_TEMP0;	// return opaque key
      break;
    }
#undef checkRW
  }

  msg->snd_code = result;
  return 1;
}

int
main()
{
  Message msg;
  server_state ss;

  init(&ss);

  capros_Process_makeStartKey(KR_SELF, keyInfo_server, KR_SCRATCH);
     
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_SCRATCH;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
     
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_CURFILE;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = ss.buf;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  msg.rcv_limit = BUF_SZ;

  do {
    RETURN(&msg);
    msg.snd_len = 0;	/* unless it's a read, in which case
			   ProcessRequest() will reset this. */
    msg.snd_invKey = KR_RETURN;
  } while ( ProcessRequest(&msg, &ss) );

  return 0;
}
