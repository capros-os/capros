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
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "xmalloc.h"
#include "PtrVec.h"

const unsigned growBy = 64;

static void
vec_MaybeGrow(PtrVec *vec)
{
  unsigned u;
  unsigned sz = vec->size;
  
  if (vec->maxSize == vec->size) {
    void **newElements;
    vec->maxSize += growBy;

    newElements = VMALLOC(void *, vec->maxSize);

    for (u = 0; u < sz; u++)
      newElements[u] = vec->elements[u];

    for (u = sz; u < vec->maxSize; u++)
      newElements[u] = 0;

    free(vec->elements);
    vec->elements = newElements;
  }
}

void
vec_reset(void *v)
{
  unsigned i;
  PtrVec *vec = v;
  
  for (i = 0; i < vec->size; i++)
    vec->elements[i] = 0;
    
  vec->size = 0;
}
  
PtrVec *
ptrvec_shallow_copy(PtrVec *vec)
{
  int i;
  PtrVec *newVec = ptrvec_create();
  for (i = 0; i < vec_len(vec); i++)
    ptrvec_append(newVec, vec_fetch(vec, i));

  return newVec;
}

void
ptrvec_append(PtrVec *vec, void *vp)
{
  vec_MaybeGrow(vec);
  vec->elements[vec->size++] = vp;
}

void
ptrvec_insert(PtrVec *vec, void *vp, unsigned ndx)
{
  unsigned i;
  
  assert (ndx <= vec->size);
  
  vec_MaybeGrow(vec);

  for (i = vec->size; i > ndx; i++)
    vec->elements[i] = vec->elements[i-1];

  vec->size++;

  vec->elements[ndx] = vp;
}

void
ptrvec_set(PtrVec *vec, unsigned ndx, void *vp)
{
  assert (ndx < vec->size);
  vec->elements[ndx] = vp;
}

void
vec_remove(void *vd_vec, unsigned ndx)
{
  unsigned i;
  PtrVec *vec = vd_vec;
  
  assert (ndx < vec->size);
  
  for (i = ndx; i < (vec->size - 1); i++)
    vec->elements[i] = vec->elements[i+1];

  vec->elements[vec->size - 1] = 0;
  
  vec->size--;
}

PtrVec *
ptrvec_create()
{
  PtrVec *vec = MALLOC(PtrVec);

  vec->size = 0;
  vec->maxSize = 0;
  vec->elements = 0;

  return vec;
}

void
ptrvec_destroy(PtrVec *vec)
{
  free(vec->elements);
  free(vec);
}

void
ptrvec_sort_using(PtrVec *vec, int (*cmp)(const void *, const void *))
{
  qsort(vec->elements, vec->size, sizeof(const void *), cmp);
}

bool
ptrvec_contains(PtrVec *vec, void *vp)
{
  unsigned i;

  for (i = 0; i < vec_len(vec); i++)
    if (vec_fetch(vec, i) == vp)
      return true;

  return false;
}
