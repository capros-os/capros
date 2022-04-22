/*
 * Copyright (C) 2002, The EROS Group, LLC.
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

/* Thread-friendly implementation of sha-1 cryptographic hash
   algorithm. */

#define BLKCHARS 64

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

/* NOTE: If a pointer is ever added to this structure for some
 * reason, change sha_create() to use non-atomic malloc! */
struct SHA {
  uint32_t h[5];		/* sha in progress */
  unsigned char block[BLKCHARS]; /* pending block */
  unsigned blen;		/* length of current block */
  uint64_t totlen;		/* total length so far */
  bool isFinished;
};

SHA *sha_create(void);
SHA *sha_create_from_string(const char *);
void sha_append(SHA *, unsigned len, const void *);
const char *sha_hexdigest(SHA *);
unsigned long sha_signature32(SHA *sha);
unsigned long long sha_signature64(SHA *sha);

#ifdef __cplusplus
}
#endif
