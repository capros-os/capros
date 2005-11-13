/*
 * Copyright (c) 2002, The EROS Group, LLC and Johns Hopkins
 * University. All rights reserved.
 * 
 * This software was developed to support the EROS secure operating
 * system project (http://www.eros-os.org). The latest version of
 * the OpenCM software can be found at http://www.opencm.org.
 * 
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * 3. Neither the name of the The EROS Group, LLC nor the name of
 *    Johns Hopkins University, nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stdbool.h>
#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <errno.h>
#include <assert.h>
#include "xmalloc.h"
#include "buffer.h"
#include "PtrVec.h"
#include "Diag.h"

#define BUFFER_BLOCK_SIZE 1024

#define min(a,b) ((a)<=(b) ? (a) : (b))
#define CMSET(x,y,value) x->y = value
#define CMGET(x,y) x->y
#define CMCLOBBER(x,y) x->y

struct Buffer {
  bool frozen;

  off_t bias;		/* for window buffers */
  off_t end;

  PtrVec *vec;
};


#define BLKNDX(pos) ((pos) / BUFFER_BLOCK_SIZE)
#define BLKOFF(pos) ((pos) % BUFFER_BLOCK_SIZE)

Buffer *
buffer_create(void)
{
  Buffer *buf = MALLOC(Buffer);

  buf->frozen = false;
  CMSET(buf,bias,0);
  CMSET(buf,end,0);
  CMSET(buf,vec,ptrvec_create());

  return buf;
}

void
buffer_destroy(Buffer *buf)
{
  unsigned i;
  for (i = 0; i < vec_len(buf->vec); i++) {
    void *v = vec_fetch(buf->vec, i);
    free(v);
    ptrvec_set(buf->vec, i, 0);
  }
  ptrvec_destroy(buf->vec);
  free(buf);
}

Buffer *
buffer_fromBuffer(const Buffer *in, off_t start, off_t len)
{
  Buffer *buf = buffer_create();

  buf->frozen = true;
  CMSET(buf,bias,start + CMGET(in,bias));
  CMSET(buf,end,CMGET(buf,bias) + len);
  CMSET(buf,vec,CMGET(in,vec));

  if (CMGET(buf,end) > CMGET(in,end))
    diag_fatal(1, "buffer_fromBuffer(): new buffer would overrun old");

  return buf;
}

void
buffer_freeze(Buffer *buf)
{
  buf->frozen = true;
}

off_t
buffer_length(const Buffer *buf)
{
  return CMGET(buf,end) - CMGET(buf,bias);
}

/* buffer_append() can only be applied on non-frozen buffers. Mutable
   buffers always have a bias value of zero. */
void
buffer_append(Buffer *buf, const void *vp, off_t len)
{
  off_t end = CMGET(buf,end) + len;
  unsigned char *bp = (unsigned char *) vp;

  while (BLKNDX(end) >= vec_len(CMGET(buf,vec))) {
    void * block = VMALLOC(void, BUFFER_BLOCK_SIZE);
    ptrvec_append(CMCLOBBER(buf,vec), block);
  }
  
  while (len > 0) {
    off_t start = CMGET(buf,end);
    size_t blkoff = BLKOFF(start);
    size_t take = min(BUFFER_BLOCK_SIZE - blkoff, len);
    unsigned char *block = 
      (unsigned char *) vec_fetch(CMGET(buf,vec), BLKNDX(start));

    block += blkoff;
    memcpy(block, bp, take);
    bp += take;
    CMSET(buf,end, CMGET(buf,end) + take);
    len -= take;
  }
}

void
buffer_appendString(Buffer *buf, const char *s)
{
  buffer_append(buf, s, strlen(s));
}

/* getChunk returns the next linear byte subsequence in a Buffer,
   along with the length of that sequence. It honors both the length
   restrictions on the buffer and also any windowing that has been
   applied (the bias field). */
BufferChunk
buffer_getChunk(const Buffer *buf, off_t pos, off_t len)
{
  pos += CMGET(buf,bias);

  if (pos >= CMGET(buf,end))
    diag_fatal(1, "Buffer length exceeded in buffer_getChunk\n");

  {
    BufferChunk bc;
    unsigned blkoff = BLKOFF(pos);
    off_t take = min(BUFFER_BLOCK_SIZE - blkoff, CMGET(buf,end) - pos);
    unsigned char *block = 
      (unsigned char *) vec_fetch(CMGET(buf,vec), BLKNDX(pos));

    bc.ptr = block + blkoff;
    bc.len = min(take, len);

    return bc;
  }
}

int
buffer_getc(const Buffer *buf, off_t pos)
{
  pos += CMGET(buf,bias);

  if (pos >= CMGET(buf,end))
    diag_fatal(1, "Buffer size exceeded");


  {
    unsigned char *block = 
      (unsigned char *) vec_fetch(CMGET(buf,vec), BLKNDX(pos));
    size_t blkoff = BLKOFF(pos);

    return block[blkoff];
  }
}

void 
buffer_read(const Buffer *buf, void *vp, off_t pos, off_t len)
{
  off_t end = pos + len;
  unsigned char *bp = vp;

  while (pos < end) {
    BufferChunk bc = buffer_getChunk(buf, pos, end - pos);
    assert(bc.len <= (end - pos));

    memcpy(bp, bc.ptr, bc.len);

    bp += bc.len;
    pos += bc.len;
  }
}

char *
buffer_asString(const Buffer *buf)
{
  size_t len = buffer_length(buf) + 1;
  char *s = VMALLOC(char, len);
  char *spos = s;

  off_t pos = 0;
  off_t end = buffer_length(buf);

  while (pos < end) {
    BufferChunk bc = buffer_getChunk(buf, pos, end - pos);
    assert(bc.len <= (end - pos));

    memcpy(spos, bc.ptr, bc.len);
    spos[bc.len] = 0;
    spos += bc.len;

    pos += bc.len;
  }

  return s;
}
