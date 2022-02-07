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

#include <stdbool.h>
#include <string.h>
#include "sha1.h"
#include "xmalloc.h"

#define shift(n, x) (((x) << (n)) | ((x) >> (32-(n))))

static const uint32_t k[80] = {
  0x5a827999u, 0x5a827999u, 0x5a827999u, 0x5a827999u, 0x5a827999u,
  0x5a827999u, 0x5a827999u, 0x5a827999u, 0x5a827999u, 0x5a827999u,
  0x5a827999u, 0x5a827999u, 0x5a827999u, 0x5a827999u, 0x5a827999u,
  0x5a827999u, 0x5a827999u, 0x5a827999u, 0x5a827999u, 0x5a827999u,

  0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u,
  0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u,
  0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u,
  0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u, 0x6ed9eba1u,

  0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu,
  0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu,
  0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu,
  0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu, 0x8f1bbcdcu,

  0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u,
  0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u,
  0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u,
  0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u, 0xca62c1d6u,
};

__inline__ static uint32_t
dofn(uint32_t n, uint32_t b, uint32_t c, uint32_t d)
{
  if (n <= 19)
    return ((b & c) | (~b & d));
  else if (n <= 39)
    return (b ^ c ^ d);
  else if (n <= 59)
    return (b & c) | (b & d) | (c & d);
  else
    return (b ^ c ^ d);
}


SHA *
sha_create()
{
  /* NOTE: This is okay because the sha structure contains no
   * pointers! */
  SHA *sha = MALLOC(SHA);
  
  sha->isFinished = false;
  sha->h[0] = 0x67452301u;
  sha->h[1] = 0xefcdab89u;
  sha->h[2] = 0x98badcfeu;
  sha->h[3] = 0x10325476u;
  sha->h[4] = 0xc3d2e1f0u;
  sha->blen = 0;
  sha->totlen = 0;

  return sha;
}

SHA *
sha_create_from_string(const char *s)
{
  SHA *sha = sha_create();
  sha_append(sha, strlen(s), s);

  return sha;
}

static void
sha_ProcessBlock(SHA *sha, uint32_t *wbuf)
{
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  uint32_t e;
  uint32_t w[80];

  int t;
    
  for(t = 0; t < 16; t++)
    w[t] = wbuf[t];

  for (t = 16; t < 80; t++) {
    uint32_t tmp = w[t-3] ^ w[t-8] ^ w[t-14] ^ w[t-16];
    w[t] = shift(1, tmp);
  }

  a = sha->h[0];
  b = sha->h[1];
  c = sha->h[2];
  d = sha->h[3];
  e = sha->h[4];

  for (t = 0; t < 80; t++) {
    uint32_t tmp = shift(5,a) + dofn(t,b,c,d) + e + w[t] + k[t];
    e = d;
    d = c;
    c = shift(30,b);
    b = a;
    a = tmp;
  }

  sha->h[0] += a;
  sha->h[1] += b;
  sha->h[2] += c;
  sha->h[3] += d;
  sha->h[4] += e;
}

void
sha_append(SHA * sha, unsigned len, const void *buf)
{
  const unsigned char *bp = (const unsigned char *) buf;

  if (sha->isFinished)
    return;
  
  do {
    unsigned char *blockp = &sha->block[sha->blen];
    
    while (len && sha->blen < BLKCHARS) {
      *blockp++ = *bp++;
      len--;
      sha->blen++;
      sha->totlen++;
    }

    if (sha->blen == BLKCHARS) {
      uint32_t myblock[16];
      int w;
      unsigned char *buf = sha->block;
    
      for (w = 0; w < 16; w++) {
	myblock[w] = ((uint32_t) buf[0]) << 24;
	myblock[w] |= ((uint32_t) buf[1]) << 16;
	myblock[w] |= ((uint32_t) buf[2]) << 8;
	myblock[w] |= ((uint32_t) buf[3]);

	buf += 4;
      }
	
      sha_ProcessBlock(sha, myblock);

      sha->blen = 0;
    }
  } while (len);
}

/* destroys engine */
static void
sha_Finish(SHA *sha)
{
  if (sha->isFinished)
    return;
  
  /* There may be overflow.  If so, we will need to build at least
     one, and possibly two additional blocks. We must append a 1 bit
     to the end of the bit-message, and we must also deal with
     encoding the message length. The length always goes in the last
     64 bits (8 bytes) of the last block, and we need a byte for that
     extra bit, so if the residual is less than 64 - 8 - 1 = 55, we
     can do it in one block, else we need two. */

  {
    int i;
    uint32_t myblock[16];
    uint32_t *wbuf = myblock;
    unsigned char *buf = sha->block;

    {
      uint32_t residual = sha->blen;
      
      for (i = 0; i < 16; i++)
	myblock[i] = 0;

      while (residual > 3) {
	*wbuf = ((uint32_t) buf[0]) << 24;
	*wbuf |= ((uint32_t) buf[1]) << 16;
	*wbuf |= ((uint32_t) buf[2]) << 8;
	*wbuf |= ((uint32_t) buf[3]);

	buf += 4;
	residual -= 4;
	wbuf++;
      }

      if (residual == 0)
	*wbuf = 0x80000000u;
      else if (residual == 1) {
	*wbuf = ((uint32_t) buf[0]) << 24;
	*wbuf |= 0x800000u;
      }
      else if (residual == 2) {
	*wbuf = ((uint32_t) buf[0]) << 24;
	*wbuf |= ((uint32_t) buf[1]) << 16;
	*wbuf |= 0x8000u;
      }
      else if (residual == 3) {
	*wbuf = ((uint32_t) buf[0]) << 24;
	*wbuf |= ((uint32_t) buf[1]) << 16;
	*wbuf |= ((uint32_t) buf[2]) << 8;
	*wbuf |= 0x80u;
      }
    }

    if (sha->blen <= 55) {
      myblock[14] = (uint32_t) (sha->totlen >> 32);
      myblock[15] = (uint32_t) sha->totlen;
      sha_ProcessBlock(sha, myblock);
    }
    else {
      sha_ProcessBlock(sha, myblock);

      for (i = 0; i < 16; i++)
	myblock[i] = 0;
      myblock[14] = (uint32_t) (sha->totlen >> 32);
      myblock[15] = (uint32_t) sha->totlen;

      sha_ProcessBlock(sha, myblock);
    }
  }
}

unsigned long
sha_signature32(SHA *sha)
{
  sha_Finish(sha);

  return sha->h[0];
}

unsigned long long
sha_signature64(SHA *sha)
{
  uint64_t sig;

  sha_Finish(sha);

  sig = sha->h[1];
  sig <<= 32;
  sig |= sha->h[0];

  return sig;
}

#ifdef USE_BASE16
const char *
sha_hexdigest(SHA *sha)
{
  int w;
  char digest[41];
  char *pdigest = digest;

  static const char hexchars[] = "0123456789abcdef";

  sha_Finish(sha);

  digest[40] = 0;
  
  for (w = 0; w < 5; w++) {
    uint32_t word = sha->h[w];

    int c;
    for (c = 0; c < 8; c++) {
      unsigned cur = (word & 0xfu);
      word >>= 4;
      
      *pdigest++ = hexchars[cur];
    }
  }
  
  return xstrdup(digest);
}
#else
const char *
sha_hexdigest(SHA *sha)
{
  char digest[41];
  char *pdigest = digest;
  int c;
  uint32_t word = 0;

  static const char b64chars[] = 
    "ABCDEFGHIJKLMNOP"
    "QRSTUVWXYZabcdef"
    "ghijklmnopqrstuv"
    "wxyz0123456789+-";	/* not slash! */

  sha_Finish(sha);

  /* First word: */
  word = sha->h[0];

  for (c = 0; c < 6; c++) {
    unsigned cur = (word & 0x3fu);
    word >>= 6;
      
    *pdigest++ = b64chars[cur];
  }

  /* two residual bits. Begin word 1: */
  word |= (sha->h[1] << 2);
  word &= 0x3fu;
  *pdigest++ = b64chars[word];

  /* Grab remaining 28 bits in word 1: */
  word = (sha->h[1] >> 4);

  for (c = 0; c < 5; c++) {
    unsigned cur = (word & 0x3fu);
    word >>= 6;
      
    *pdigest++ = b64chars[cur];
  }

  /* four residual bits. Begin word 2: */
  word |= (sha->h[2] << 4);
  word &= 0x3fu;
  *pdigest++ = b64chars[word];

  /* Grab remaining 30 bits in word 2: */
  word = (sha->h[2] >> 2);
  for (c = 0; c < 6; c++) {
    unsigned cur = (word & 0x3fu);
    word >>= 6;
      
    *pdigest++ = b64chars[cur];
  }

  /* no residual bits. Begin word 3: */
  word = sha->h[3];
  for (c = 0; c < 5; c++) {
    unsigned cur = (word & 0x3fu);
    word >>= 6;
      
    *pdigest++ = b64chars[cur];
  }

  /* two residual bits. Begin word 4: */
  word |= (sha->h[4] << 2);
  word &= 0x3fu;
  *pdigest++ = b64chars[word];

  /* Grab remaining 28 bits in word 4: */
  word = (sha->h[4] >> 4);
  for (c = 0; c < 5; c++) {
    unsigned cur = (word & 0x3fu);
    word >>= 6;
      
    *pdigest++ = b64chars[cur];
  }

  *pdigest = 0;
  
  return strdup(digest);
}
#endif
