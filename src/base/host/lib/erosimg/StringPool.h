#ifndef __STRINGPOOL_H__
#define __STRINGPOOL_H__
/*
 * Copyright (C) 1998, 1999, 2002 Jonathan S. Shapiro.
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

/* A StringPool manages a dense collection of unique strings */

typedef struct StringPool StringPool;

struct StringPool {
  char *pool;
  int poolsz;
  int poolLimit;
  
  struct PoolEntry **poolTable;
};

#ifdef __cplusplus
extern "C" {
#endif
  StringPool *strpool_create();
  void strpool_destroy(StringPool *pStrPool);

  int strpool_Add(StringPool *, const char *);
  const char *strpool_Get(const StringPool *, int offset);

  INLINE int 
  strpool_Size(const StringPool *pStrPool)
  { 
    return pStrPool->poolsz; 
  }

  INLINE const char *
  strpool_GetPoolBuffer(const StringPool *pStrPool)
  {
    return pStrPool->pool;
  }

  /* returns number of bytes written */
  int strpool_WriteToFile(const StringPool*, int fd);

  bool strpool_ReadFromFile(StringPool *, int fd, int len);
#ifdef __cplusplus
}
#endif

#endif /* __STRINGPOOL_H__ */
