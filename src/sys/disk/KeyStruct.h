#ifndef __DISK_KEYSTRUCT_H__
#define __DISK_KEYSTRUCT_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
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

#include <disk/ErosTypes.h>
#ifdef __KERNEL__
#include <eros/Link.h>
#endif

typedef unsigned KeyType;

typedef struct KeyBits KeyBits;

struct KeyBits {
  union {
    struct {
      OID_s oid;
      ObCount count;
    } unprep;

#ifdef __KERNEL__
    struct {
      Link kr;
      struct ObjectHeader *pObj;
    } ok;
    struct {
      Link kr;
      struct Process *pContext;
    } gk;
#endif

    struct {			/* NUMBER KEYS */
      uint32_t value[3];
    } nk;

    /* Priority keys use the keyData field, but no other fields. */
    
    /* Miscellaneous Keys have no substructure, but use the 'keyData'
     * field.
     */

    /* Device Keys (currently) have no substructure, but use the
     * 'keyData' field. 
     */

    /* It is currently true that oidLo and oidHi in range keys and
     * object keys overlap, but not important that they do so.
     */

    struct {			/* RANGE KEYS */
      OID_s oid;
      uint32_t count;
    } rk;      
    
    struct {			/* DEVICE KEYS */
      uint16_t devClass;
      uint16_t devNdx;
      uint32_t devUnit;
    } dk;
  } u;

  /* EVERYTHING BELOW HERE IS GENERIC TO ALL KEYS.  Structures above
   * this point should describe the layout of a particular key type
   * and should occupy exactly three words.
   */
  
#ifdef BITFIELD_PACK_LOW
  uint8_t keyType;
  uint8_t keyFlags : 3;
  uint8_t keyPerms : 5;
  uint16_t keyData;
#else
#error "verify bitfield layout" 
#endif
} ;

#endif /* __DISK_KEYSTRUCT_H__ */
