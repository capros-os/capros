/*
 * Copyright (C) 1998, 1999, 2002, Jonathan S. Shapiro.
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

#include <string.h>
#include <unistd.h>
#include <malloc.h>

#include <erosimg/App.h>
#include <erosimg/Intern.h>
#include <erosimg/StringPool.h>

enum { nPockets = 256 };
  
typedef struct PoolEntry PoolEntry;
struct PoolEntry {
  uint32_t signature;
  int offset;			/* into pool buffer */
  PoolEntry *pNext;
};
  
StringPool *
strpool_create()
{
  int i;
  StringPool *pStrPool = (StringPool *) malloc(sizeof(StringPool));
  pStrPool->poolTable = (PoolEntry **) malloc(sizeof(PoolEntry *) * nPockets);

  for (i = 0; i < nPockets; i++)
    pStrPool->poolTable[i] = 0;
  
  pStrPool->poolsz = 0;
  pStrPool->poolLimit = 0;
  pStrPool->pool = 0;

  strpool_Add(pStrPool, intern(""));	/* empty string is ALWAYS first! */

  return pStrPool;
}

void
strpool_destroy(StringPool *pStrPool)
{
  int i;

  for (i = 0; i < nPockets; i++) {
    PoolEntry* pe = pStrPool->poolTable[i];

    while (pe) {
      PoolEntry *npe = pe->pNext;
      free(pe);
      pe = npe;
    }
  }

  free(pStrPool->poolTable);
  free(pStrPool->pool);
  free(pStrPool);
}

static void
strpool_reinit(StringPool *pStrPool)
{
  if (pStrPool->pool) {
    int i;

    for (i = 0; i < nPockets; i++) {
      PoolEntry* pe = pStrPool->poolTable[i];
      pStrPool->poolTable[i] = 0;
      
      while (pe) {
	PoolEntry *npe = pe->pNext;
	free(pe);
	pe = npe;
      }
    }

    free(pStrPool->pool);

    pStrPool->poolsz = 0;
    pStrPool->poolLimit = 0;
    pStrPool->pool = 0;
  }

  strpool_Add(pStrPool, intern(""));	/* empty string is ALWAYS first! */
}

static int
strpool_Lookup(StringPool *pStrPool, const char *s)
{
  unsigned sig = intern_gensig(s, strlen(s));
  int pocket = sig % nPockets;
  PoolEntry *pe = pStrPool->poolTable[pocket];

  while (pe) {
    if (pe->signature == sig &&
	strcmp(s, pStrPool->pool + pe->offset) == 0)
      return pe->offset;

    pe = pe->pNext;
  }
  return -1;
}

int 
strpool_Add(StringPool *pStrPool, const char *s)
{
  unsigned sig = intern_gensig(s, strlen(s));
  int pos = strpool_Lookup(pStrPool, s);
  int pocket = sig % nPockets;
  PoolEntry *pe;
  size_t len;
  size_t need;
  size_t avail;
  
  if (pos != -1)
    return pos;

  pe = (PoolEntry *) malloc(sizeof(PoolEntry));
  pe->signature = sig;
  pe->offset = pStrPool->poolsz;
  pe->pNext = pStrPool->poolTable[pocket];
  pStrPool->poolTable[pocket] = pe;

  /* Now copy the string! */
  len = strlen(s);

  need = len + 1;		/* terminating null! */
  avail = pStrPool->poolLimit - pStrPool->poolsz;

  if (avail < need) {
    char *newPool;

    while (avail < need) {
      pStrPool->poolLimit += 4096;
      avail = pStrPool->poolLimit - pStrPool->poolsz;
    }
    
    newPool = (char *) malloc(sizeof(char) * pStrPool->poolLimit);

    /* buffer contains nulls, so must use memcpy!! */
    if (pStrPool->pool)
      memcpy(newPool, pStrPool->pool, pStrPool->poolsz); 

    free(pStrPool->pool);
    pStrPool->pool = newPool;
  }

  pos = pStrPool->poolsz;

  strcpy(pStrPool->pool + pos, s); /* copies trailing null */
  pStrPool->poolsz += need;

/*  Diag::printf("Pool 0x08%x: add string \"%s\"\n", this, s); */
  return pos;
}

const char *
strpool_Get(const StringPool *pStrPool, int offset)
{
  if (offset < pStrPool->poolsz)
    return intern(&pStrPool->pool[offset]);
  return 0;
}

int
strpool_WriteToFile(const StringPool *pStrPool, int fd)
{
  return write(fd, pStrPool->pool, pStrPool->poolsz);
}

/* This takes advantage of the fact that the logic for adding new
 * strings is append-always.
 */
bool
strpool_ReadFromFile(StringPool *pStrPool, int sfd, int len)
{
  char *tmpPool = (char *) malloc(sizeof(char) * len);
  int offset = 0;

  strpool_reinit(pStrPool);
  
  if ( read(sfd, tmpPool, len) != len )
    return false;

  /* Run through the temporary pool, interning all the strings and
   * adding them to the current pool.  This works only because the
   * pool implementation is append-only and we are starting from an
   * empty pool:
   */

  while (offset < len) {
    const char *s = intern(&tmpPool[offset]);
    strpool_Add(pStrPool, s);

    offset += strlen(s);
    offset += 1;
  }

  return true;
}
