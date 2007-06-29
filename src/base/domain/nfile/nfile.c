/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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
#include <eros/target.h>
#include <domain/Runtime.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>

#include <idl/eros/key.h>
#include <idl/eros/Number.h>

#include <domain/VcskKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/ConstructorKey.h>
#include <domain/NFileKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <string.h>
#include <stdlib.h>

#include "constituents.h"

#include <forwarder.h>

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
#if 0
#define dbg_flags   ( dbg_alloc|dbg_read|dbg_inogrow|dbg_req )
#else
#define dbg_flags   ( 0u )
#endif

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_FILESTART  KR_APP(0)	/* start key for files */
#define KR_CURFILE    KR_APP(1)
#define KR_OSTREAM    KR_APP(2)
#define KR_MYSPACE    KR_APP(3)
#define KR_SCRATCH    KR_APP(4)

/* #define FLIP_BUF */
#ifdef FLIP_BUF
#define BUF_SZ  EROS_PAGE_SIZE
#else
#define BUF_SZ  EROS_MESSAGE_LIMIT
#endif
#define BLOCK_SIZE 4096

#define NSTACK (BUF_SZ/EROS_PAGE_SIZE + 1)
const uint32_t __rt_stack_pages = NSTACK;

#define GROW_NOZERO 2
#define GROW        1
#define NO_GROW     0

/* Following is temporary!!! */
typedef uint32_t f_size_t;

/* Storage begins at base_addr and grows upwards (without limit!). */
#define base_addr ((uint8_t *) 0x08000000)

#define INO_NINDIR 11

/* Following structure is of a size that evenly divides
   BLOCK_SIZE, which is important to the implementation */
typedef struct ino ino_s;
struct ino {
  union {
    uint64_t sz;
    /* 3 bytes available here */
    ino_s    *nxt_free;
  } u;
  uint64_t uuid;		/* unique ID */
  uint8_t  nLayer;
  uint32_t *indir[INO_NINDIR];
} ;

/* Max of 5 indirection blocks */
const uint64_t sizes_by_layers[] = {
  1llu * INO_NINDIR * BLOCK_SIZE,
  1llu * INO_NINDIR * BLOCK_SIZE
    * (BLOCK_SIZE/sizeof(uint32_t *)),
  1llu * INO_NINDIR * BLOCK_SIZE 
    * (BLOCK_SIZE/sizeof(uint32_t *))
    * (BLOCK_SIZE/sizeof(uint32_t *)),
  1llu * INO_NINDIR * BLOCK_SIZE 
    * (BLOCK_SIZE/sizeof(uint32_t *))
    * (BLOCK_SIZE/sizeof(uint32_t *))
    * (BLOCK_SIZE/sizeof(uint32_t *)),
  1llu * INO_NINDIR * BLOCK_SIZE 
    * (BLOCK_SIZE/sizeof(uint32_t *))
    * (BLOCK_SIZE/sizeof(uint32_t *))
    * (BLOCK_SIZE/sizeof(uint32_t *))
    * (BLOCK_SIZE/sizeof(uint32_t *)),
};


typedef struct server_state {
  uint8_t *buf;

  ino_s    *first_free_inode;
  uint32_t *first_free_block;
  ino_s    root;
  uint8_t *top_addr;
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

  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);

  /* Buy a new root node: */
  result = spcbank_buy_nodes(KR_BANK, 1, KR_MYSPACE, KR_VOID, KR_VOID);
  if (result != RC_OK)
    DEBUG(init) kdprintf(KR_OSTREAM, "DIR: spcbank nodes exhausted\n", result);
  
  DEBUG(init)
    kdprintf(KR_OSTREAM, "Bought new space\n");

  /* make that node LSS=TOP_LSS */
  node_make_node_key(KR_MYSPACE, EROS_ADDRESS_BLSS, 0, KR_MYSPACE);

  node_swap(KR_MYSPACE, 0, KR_SCRATCH, KR_VOID);

  process_swap(KR_SELF, ProcAddrSpace, KR_MYSPACE, KR_VOID);
  
  node_copy(KR_CONSTIT, KC_ZSF, KR_SCRATCH);
  result = constructor_request(KR_SCRATCH, KR_BANK, KR_SCHED,
			       KR_VOID, KR_SCRATCH);

  /* plug in newly allocated ZSF */
  DEBUG(init) kdprintf(KR_OSTREAM, 
		       "FS: plugging zsf into new spc root\n", result);
  node_swap(KR_MYSPACE, 1, KR_SCRATCH, KR_VOID);

  bzero(ss, sizeof(*ss));
  ss->top_addr = base_addr;
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
 */
uint32_t **
find_file_page(server_state *ss, ino_s *ino, f_size_t at, int wantGrow)
{
  uint64_t allocSz = (ino->u.sz + (BLOCK_SIZE - 1)) & (BLOCK_SIZE-1);

  uint64_t atPg = (at / BLOCK_SIZE);
  uint32_t ndx;
  uint32_t **blockTable = ino->indir;
    
  DEBUG(findpg)
    kdprintf(KR_OSTREAM, "find: ino: 0x%X sz 0x%X at: 0x%x, grow? %c\n",
	    ino->uuid, ino->u.sz, at, (wantGrow == GROW) ? 'y' : 'n');
  
  /* Grow the file upwards as necessary. */
  while (allocSz > sizes_by_layers[ino->nLayer]) {
    int i;
    uint32_t **newIndir;
      
    DEBUG(findpg)
      kdprintf(KR_OSTREAM, "find: ino: 0x%X allocSz 0x%X nLayer %d (max 0x%x) => grow\n",
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
    kdprintf(KR_OSTREAM, "find: ino: 0x%X nLayer: %d\n",
	    ino->uuid, ino->nLayer);
  
  {
    uint32_t layer = ino->nLayer;

    while (layer > 0) {
      ndx =
	(atPg / (layer * (BLOCK_SIZE/sizeof(uint32_t *))));
      ndx %= (BLOCK_SIZE / sizeof(uint32_t));
    
      DEBUG(findpg)
	kdprintf(KR_OSTREAM, "find: ino: 0x%X layer %d ndx %d\n",
		ino->uuid, layer, ndx);

      blockTable = (uint32_t **)blockTable[ndx];

      DEBUG(findpg)
	kdprintf(KR_OSTREAM, "find: ino: 0x%X layer %d ndx %d ==> 0x%x\n",
		ino->uuid, layer, ndx, blockTable);

      if (blockTable == 0) {
	if (wantGrow == NO_GROW)
	  return 0;

	blockTable[ndx] = (uint32_t *) AllocBlock(ss, WANT_ZERO);
	blockTable = (uint32_t **)blockTable[ndx];

	DEBUG(findpg)
	  kdprintf(KR_OSTREAM, "find: ino: 0x%X layer %d ndx %d: grow layer: 0x%x\n",
		  ino->uuid, layer, ndx, blockTable);
      }
    
      layer--;
    }

    ndx = atPg % (BLOCK_SIZE / sizeof(uint32_t));

    DEBUG(findpg)
      kdprintf(KR_OSTREAM, "find: ino: 0x%X layer %d ndx %d: blockTbl 0x%x bt[ndx] 0x%x\n",
	       ino->uuid, layer, ndx, blockTable, blockTable[ndx]);

    if (blockTable[ndx] == 0 && wantGrow != NO_GROW) {
      DEBUG(findpg)
	kdprintf(KR_OSTREAM, "find: ino: 0x%X layer %d ndx %d: grow leaf: 0x%x\n",
		 ino->uuid, layer, ndx, blockTable[ndx]);

      if (wantGrow == GROW)
	blockTable[ndx] = (uint32_t *) AllocBlock(ss, WANT_ZERO);
      else
	blockTable[ndx] = (uint32_t *) AllocBlock(ss, NO_ZERO);
    }
  }
  
  DEBUG(findpg)
    kdprintf(KR_OSTREAM, "find: ino: 0x%X return 0x%x contains 0x%x\n",
	    ino->uuid, &blockTable[ndx], blockTable[ndx]);

  return &blockTable[ndx];
}

/* Write /len/ bytes of data from /buf/ into /file/, starting at
   position /at/.  Extends the file as necessary. */
uint32_t
write_to_file(server_state *ss, ino_s *ino, f_size_t at,
	      uint32_t len, uint8_t *buf)
{
  DEBUG(write)
    kdprintf(KR_OSTREAM, "write: ino: 0x%X writing %d at 0x%x\n",
	    ino->uuid, len, at);

  /* The passed /buf/ is contiguous, but there is no guarantee that
     the file itself is. */
  while (len) {
    uint32_t offset = at & (BLOCK_SIZE - 1);
    uint32_t nBytes = BLOCK_SIZE - offset;

    DEBUG(write)
      kdprintf(KR_OSTREAM, "write: ino: 0x%X bwrite %d at 0x%x\n",
	       ino->uuid, nBytes, at);

    if (nBytes > len)
      nBytes = len;

    {
      uint32_t grow =
	(offset == 0 && nBytes == BLOCK_SIZE) ? GROW_NOZERO : GROW;
      
      uint32_t **ppPage = find_file_page(ss, ino, at, grow);
      uint8_t *pPage = (uint8_t *) *ppPage;

#ifdef FLIP_BUF
      if (offset == 0 && nBytes == BLOCK_SIZE) {
	uint8_t *tmp = pPage;
	*ppPage = (uint32_t *) ss->buf;
	ss->buf = tmp;
      }
      else
#endif
#if 1
	bcopy(buf, &pPage[offset], nBytes);
#endif

      DEBUG(write)
	kdprintf(KR_OSTREAM, "write: ino: 0x%X bwrote %d at 0x%x\n",
		 ino->uuid, nBytes, at);

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
uint32_t
read_from_file(server_state *ss, ino_s *ino, f_size_t at,
	       uint32_t *rqLen, uint8_t *buf)
{
  uint32_t len = *rqLen;
  
  DEBUG(read)
    kdprintf(KR_OSTREAM, "read: ino: 0x%X reading %d at 0x%x\n",
	    ino->uuid, len, at);

  if (at > ino->u.sz)
    return RC_eros_key_RequestError;

  if (at + len > ino->u.sz) {
    len = ino->u.sz - at;
    *rqLen = len;

    DEBUG(read)
      kdprintf(KR_OSTREAM, "read: ino: 0x%X truncated to %d at 0x%x\n",
	       ino->uuid, len, at);
  }
  
  /* The passed /buf/ is contiguous, but there is no guarantee that
     the file itself is. */
  while (len) {
    uint32_t offset = at & (BLOCK_SIZE - 1);
    uint32_t nBytes = BLOCK_SIZE - offset;

    if (nBytes > len)
      nBytes = len;

    DEBUG(write)
      kdprintf(KR_OSTREAM, "read: ino: 0x%X bread %d at 0x%x\n",
	       ino->uuid, nBytes, at);

    {
      uint32_t **ppPage = find_file_page(ss, ino, at, NO_GROW);

      if (ppPage == 0) {
	DEBUG(write)
	  kdprintf(KR_OSTREAM, "read: ino: 0x%X bread %d at 0x%x -- lazy zero\n",
		   ino->uuid, nBytes, at);

	bzero(buf, nBytes);
      }
      else {
	uint8_t *pPage = (uint8_t *) *ppPage;

	bcopy(&pPage[offset], buf, nBytes);
      }
    }

    DEBUG(write)
      kdprintf(KR_OSTREAM, "read: ino: 0x%X got i\n", ino->uuid);

    /* blockTable now points to the start of the content page */
    len -= nBytes;
    at += nBytes;
    buf += nBytes;
  }

  return RC_OK;
}

void
grow_inode_table(server_state *ss)
{
  int i;
  
  DEBUG(inogrow)
    kdprintf(KR_OSTREAM, "ino: growing inode table\n");
  
  /* Extends the file with a new zero page: */
  uint32_t ** ppPage = find_file_page(ss, &ss->root, ss->root.u.sz, GROW);
  ino_s * pIno = (ino_s *) *ppPage;
  
  DEBUG(inogrow)
    kdprintf(KR_OSTREAM, "ino: pIno=0x%x, %d, %d\n", pIno, BLOCK_SIZE, sizeof(ino_s));

  for (i = 0; i < BLOCK_SIZE / sizeof(ino_s); i++)
    pIno[i].u.nxt_free = &pIno[i+1];

  pIno[i-1].u.nxt_free = 0;

  ss->first_free_inode = &pIno[0];
}

uint32_t
create_new_file(server_state *ss, ino_s** outFile)
{
  ino_s    *newfile;
  
  if (ss->first_free_inode == 0)
    grow_inode_table(ss);

  /* Now have at least one free inode */

  newfile = ss->first_free_inode;
  ss->first_free_inode = newfile->u.nxt_free;

  bzero(newfile, sizeof(*newfile));
  *outFile = newfile;
  newfile->nLayer = 0;
  newfile->uuid = ss->nxt_uuid;
  ss->nxt_uuid++;
  
  DEBUG(ino)
    kdprintf(KR_OSTREAM, "ino: created new file ino=0x%x with uuid 0x%X\n",
	     newfile, newfile->uuid);
  
  return RC_OK;
}

void
reclaim_ino_pages(server_state *ss, uint32_t lvl, uint32_t *blockTable)
{
  if (lvl) {
    int i;
    for (i = 0; i < BLOCK_SIZE/sizeof(uint32_t); i++)
      reclaim_ino_pages(ss, lvl - 1, (uint32_t *) blockTable[i]);
  }

  
  if (blockTable) {
    DEBUG(free)
      kdprintf(KR_OSTREAM, "Freeing pg 0x%x\n", blockTable);
  
    *blockTable = (uint32_t) ss->first_free_block;
    ss->first_free_block = blockTable;
  }
}

uint32_t
destroy_file(server_state *ss, ino_s *ino)
{
  int i;
  uint32_t result;
  
  result = spcbank_return_node(KR_BANK, KR_CURFILE);

  for (i = 0; i < INO_NINDIR; i++)
    reclaim_ino_pages(ss, ino->nLayer, ino->indir[i]);

  ino->u.nxt_free = ss->first_free_inode;
  ss->first_free_inode = ino;
  
  return result;
}

int
ProcessRequest(Message *msg, server_state *ss)
{
  uint32_t result = RC_OK;
  
  msg->snd_key0 = KR_VOID;
  msg->snd_len = 0;		/* until proven otherwise */
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;

  switch(msg->rcv_code) {
  case OC_NFile_Create:
    {
      ino_s *newFile;

      DEBUG(req)
	kdprintf(KR_OSTREAM, "NFILE: create\n");

      result = create_new_file(ss, &newFile);
      if (result != RC_OK)
	break;

      result = forwarder_create(KR_BANK, KR_CURFILE, KR_SCRATCH, KR_FILESTART,
                 eros_Forwarder_sendWord,
                 (uint32_t)newFile);
      if (result != RC_OK)
	break;

      msg->snd_key0 = KR_CURFILE;
      break;
    }
  case OC_NFile_Destroy:
    {
      ino_s *ino = (ino_s *)msg->rcv_w3;

      DEBUG(req)
	kdprintf(KR_OSTREAM, "NFILE: ino 0x%X destroy\n",
		 ino->uuid);

      result = destroy_file(ss, ino);

      break;
    }
    
  case OC_NFile_Read:
    {
      ino_s *ino = (ino_s *)msg->rcv_w3;
      f_size_t len = msg->rcv_w1;
      f_size_t at = msg->rcv_w2;
      
      DEBUG(req)
	kdprintf(KR_OSTREAM, "NFILE: ino 0x%X read %d at %d\n",
		 ino->uuid, len, at);

      result = read_from_file(ss, ino,  at, &len, ss->buf);
      msg->snd_data = &ss->buf;
      msg->snd_len = len;
      break;
    }

  case OC_NFile_Write:
    {
      ino_s *ino = (ino_s *)msg->rcv_w3;
      f_size_t len = min(msg->rcv_limit, msg->rcv_sent);
      f_size_t at = msg->rcv_w2;
      
      DEBUG(req)
	kdprintf(KR_OSTREAM, "NFILE: ino 0x%X write %d at %d\n",
		 ino->uuid, len, at);

#if 1
      result = write_to_file(ss, ino,  at, len, ss->buf);
#endif

      msg->snd_w1 = len;
      break;
    }

  default:
    DEBUG(req)
      kdprintf(KR_OSTREAM, "NFILE: unknown request %x (%d)\n",
	       msg->rcv_code, msg->rcv_code);

    result = RC_eros_key_UnknownRequest;
    break;
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

  process_make_start_key(KR_SELF, 0, KR_SCRATCH);
  process_make_start_key(KR_SELF, 1, KR_FILESTART);
     
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
#ifdef FLIP_BUF
    msg.rcv_data = ss.buf;
#endif
    RETURN(&msg);
    msg.snd_len = 0;	/* unless it's a read, in which case
			   ProcessRequest() will reset this. */
    msg.snd_invKey = KR_RETURN;
  } while ( ProcessRequest(&msg, &ss) );

  return 0;
}
