/*
 * Copyright (C) 1998, 1999, 2001, 2002, Jonathan S. Shapiro.
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

#include <memory.h>

#include <erosimg/DiskKey.h>

void
init_NumberKey(KeyBits *dk, uint32_t first, uint32_t second, uint32_t third)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Number);
  keyBits_SetUnprepared(dk);
  keyBits_SetReadOnly(dk);

  dk->u.nk.value[0] = first;
  dk->u.nk.value[1] = second;
  dk->u.nk.value[2] = third;
}

void
init_MiscKey(KeyBits *dk, uint16_t ty)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, ty);
#ifdef KT_TimePage
  if (ty == KT_TimePage) {
    keyBits_SetBlss(dk, EROS_PAGE_BLSS);
    keyBits_SetReadOnly(dk);
  }
  else
#endif
    dk->keyData = 0;
  
  dk->u.nk.value[2] = 0;
  dk->u.nk.value[1] = 0;
  dk->u.nk.value[0] = 0;
};


void
init_SchedKey(KeyBits *dk, uint16_t prio)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Sched);
  keyBits_SetUnprepared(dk);
  dk->keyFlags = 0;
  dk->keyPerms = 0;
  dk->keyData = prio;
  
  dk->u.nk.value[2] = 0;
  dk->u.nk.value[1] = 0;
  dk->u.nk.value[0] = 0;
};

void
init_RangeKey(KeyBits *dk, OID oidlo, OID oidhi)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Range);
  keyBits_SetUnprepared(dk);
  dk->keyFlags = 0;
  dk->keyPerms = 0;
  dk->keyData = 0;

  dk->u.rk.oid = oidlo;
  dk->u.rk.count = oidhi - oidlo;
}

void
init_NodeKey(KeyBits *dk, OID oid, uint16_t keyData)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Node);
  keyBits_SetUnprepared(dk);
  dk->keyFlags = 0;
  dk->keyPerms = 0;
  dk->keyData = keyData;
  
  dk->u.unprep.oid = oid;
  dk->u.unprep.count = 0;
}

void
init_StartKey(KeyBits *dk, OID oid, uint16_t keyData)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Start);
  keyBits_SetUnprepared(dk);
  dk->keyFlags = 0;
  dk->keyPerms = 0;
  dk->keyData = keyData;
  
  dk->u.unprep.oid = oid;
  dk->u.unprep.count = 0;
}

void 
init_ResumeKey(KeyBits *dk, OID oid)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Resume);
  keyBits_SetUnprepared(dk);
  dk->keyFlags = 0;
  dk->keyPerms = 0;
  dk->keyData = 0;
  
  dk->u.unprep.oid = oid;
  dk->u.unprep.count = 0;
}

void 
init_ProcessKey(KeyBits *dk, OID oid)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Process);
  keyBits_SetUnprepared(dk);
  dk->keyFlags = 0;
  dk->keyPerms = 0;
  dk->keyData = 0;
  
  dk->u.unprep.oid = oid;
  dk->u.unprep.count = 0;
}

void
init_DataPageKey(KeyBits *dk, OID oid, bool readOnly)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Page);
  keyBits_SetUnprepared(dk);
  dk->keyFlags = 0;
  dk->keyPerms = 0;
  dk->keyData = EROS_PAGE_BLSS;
  if (readOnly)
    keyBits_SetReadOnly(dk);
  
  dk->u.unprep.oid = oid;
  dk->u.unprep.count = 0;
}

void 
init_SegmentKey(KeyBits *dk, OID oid, uint32_t lss, bool readOnly, bool noCall)
{
  keyBits_InitToVoid(dk);
  keyBits_InitType(dk, KKT_Segment);
  keyBits_SetUnprepared(dk);
  dk->keyFlags = 0;
  dk->keyPerms = 0;
  dk->keyData = lss;
  if (readOnly)
    keyBits_SetReadOnly(dk);
  if (noCall)
    keyBits_SetNoCall(dk);
  
  dk->u.unprep.oid = oid;
  dk->u.unprep.count = 0;
}

#if 0
DiskKey::DiskKey(const KeyBits& that)
{
  memcpy(this, &that, sizeof(KeyBits));
}
#endif
