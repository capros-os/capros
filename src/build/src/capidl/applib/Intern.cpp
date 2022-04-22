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

#include <string.h>
#include <malloc.h>
#include "Intern.h"
#include "xmalloc.h"

struct InternEntry {
  InternEntry *pNext;
  unsigned signature;
  char string[0];		/* variable size! */
};
  
static InternEntry **internTable;

unsigned
intern_gensig(const char* s, int len)
{
  int i;
  unsigned sig = 0;

  for (i = 0; i < (len + 1)/2; i++) {
    sig = sig * 27 + s[i];
    sig = sig * 27 + s[len - 1 - i];
  }

  return (unsigned) sig;
}

static InternEntry *
doLookup(const char* s, int len, unsigned strsig)
{
  unsigned ndx = strsig % 256;

  InternEntry *pEntry = internTable[ndx];

  while(pEntry) {
    if (pEntry->signature == strsig) {
      if ((strncmp(pEntry->string, s, len) == 0)
	  && (pEntry->string[len] == 0))
	return pEntry;
    }

    pEntry = pEntry->pNext;
  }

  return 0;
}

const char *
intern(const char *s)
{
  if (s)
    return internWithLength(s, strlen(s));
  else
    return 0;
}

const char *
internWithLength(const char* s, int len)
{
  InternEntry *pEntry;
  unsigned sig = intern_gensig(s, len);
  unsigned ndx = sig % 256;

  if (!internTable) {
    int i;
    internTable = (InternEntry **) malloc(sizeof(InternEntry*) * 256);
    for (i = 0; i < 256; i++)
      internTable[i] = 0;
  }

  if ( (pEntry = doLookup(s, len, sig)) == 0 ) {
    pEntry = (InternEntry *)malloc(sizeof(InternEntry) + len + 1);
    pEntry->signature = sig;
    pEntry->string[len] = 0;
    memcpy(pEntry->string, s, len);

    pEntry->pNext = internTable[ndx];
    internTable[ndx] = pEntry;
  }

  return pEntry->string;
}

const char *
intern_concat(const char *s1, const char *s2)
{
  size_t l1 = strlen(s1);
  size_t l2 = strlen(s2);
  char *s = VMALLOC(char, l1+l2+1);
  const char *is;

  strcpy(s, s1);
  strcat(s, s2);

  is = intern(s);
  free(s);
  return is;
}
