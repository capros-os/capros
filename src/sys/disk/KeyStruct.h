#ifndef __DISK_KEYSTRUCT_H__
#define __DISK_KEYSTRUCT_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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

#include <disk/ErosTypes.h>
#ifdef __KERNEL__
#include <kerninc/Link.h>
#endif

typedef unsigned KeyType;

typedef struct KeyBits KeyBits;

struct KeyBits {
  union {
    struct {
      OID     oid;
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
      OID     oid;
      uint32_t    count;
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
