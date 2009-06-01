#ifndef __DISK_DISKNODE_H__
#define __DISK_DISKNODE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#ifndef __ASSEMBLER__
/* This structure defines the *layout* of the disk node structure so
 * that various elements of the kernel can fetch things from ROM and
 * RAM nodes. 
 *
 * For the moment, the kernel still makes the assumption that nodes on
 * disk are gathered into node pots -- even in ROM and RAM
 * images. This should perhaps be reconsidered, so as to isolate the
 * kernel from unnecessary knowledge.
 */

#include "KeyStruct.h"

typedef struct DiskNode {
  OID_s oid;
  ObCount allocCount;
  ObCount callCount;

  uint16_t nodeData;
  KeyBits slot[EROS_NODE_SIZE];
} DiskNode;

#define DISK_NODES_PER_PAGE (EROS_PAGE_SIZE / sizeof(DiskNode))

INLINE uint8_t *
proc_runStateField(DiskNode * dn)
{
  /* N.B. This must match the location in the LAYOUT file. */
  return ((uint8_t *) &dn->slot[8].u.nk.value) + 8;
}

#endif // __ASSEMBLER__

#endif /* __DISK_DISKNODE_H__ */
