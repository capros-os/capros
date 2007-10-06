/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/GPT.h>
#include <kerninc/Activity.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/GPT.h>

static void
InvalidateMaps(GPT * theGPT)
{
  /* Invalidate any mapping table entries that depended on the l2v. */
  /* node_Unprepare also invalidates all products of this GPT.
     This is necessary in the case where the GPT produced a small space,
     but can no longer do so.
     It may be unnecessary in other cases, but this seems like a good time
     to clean them up, since they may very well be useless. */
#ifdef NDEBUG	// avoid compiler warning
  (void) node_Unprepare(theGPT, 0);
#else
  bool unprepRet = node_Unprepare(theGPT, 0);
  assert(unprepRet);
#endif
  keyR_UnmapAll(&node_ToObj(theGPT)->keyRing);
}

void
DoMemoryReduce(Invocation * inv)
{
  COMMIT_POINT();

  uint32_t w = inv->entry.w1; /* the restrictions */
  if (w & ~(capros_Memory_opaque
            | capros_Memory_weak
            | capros_Memory_noCall
            | capros_Memory_readOnly )) {
    inv->exit.code = RC_capros_key_RequestError;
    dprintf(true, "restr=0x%x\n", w);
    return;
  }
    
  if (inv->exit.pKey[0]) {
    inv_SetExitKey(inv, 0, inv->key);
    inv->exit.pKey[0]->keyPerms = w | inv->key->keyPerms;
  }

  inv->exit.code = RC_OK;
  return;
}

/* May Yield. */
void
GPTKey(Invocation * inv)
{
  GPT * theGPT = (GPT *) key_GetObjectPtr(inv->key);
  bool opaque = inv->key->keyPerms & capros_Memory_opaque;

  switch (inv->entry.code) {

  case OC_capros_Memory_getRestrictions:
    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = inv->key->keyPerms;
    return;

  case OC_capros_Memory_reduce:
    DoMemoryReduce(inv);
    return;

  case OC_capros_GPT_getL2v:
    {
      if (opaque) goto opaqueError;

      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = gpt_GetL2vField(theGPT) & GPT_L2V_MASK;
      return;
    }

  case OC_capros_GPT_setL2v:
    {
      if (opaque) goto opaqueError;

      InvalidateMaps(theGPT);

      COMMIT_POINT();

      unsigned int newL2v = inv->entry.w1;
      if (! (newL2v < 64 && newL2v >= EROS_PAGE_LGSIZE))
        goto request_error;

      inv->exit.code = RC_OK;
      uint8_t l2vField = gpt_GetL2vField(theGPT);
      uint8_t oldL2v = l2vField & GPT_L2V_MASK;
      // inv->exit.w1 = oldL2v;
      gpt_SetL2vField(theGPT, l2vField - oldL2v + newL2v);
      return;
    }

  case OC_capros_GPT_getSlot:
    {
      if (opaque) {
opaqueError:
        COMMIT_POINT();
request_error:
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      COMMIT_POINT();

      uint32_t slot = inv->entry.w1;
      if (slot >= capros_GPT_nSlots) {
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      /* Does not copy hazard bits, but preserves preparation: */
      inv_SetExitKey(inv, 0, &theGPT->slot[slot]);
      // FIXME: Implement weak? 

      inv->exit.code = RC_OK;
      return;
    }

  case OC_capros_GPT_setSlot:
    {
      if (opaque) goto opaqueError;

      uint32_t slot = inv->entry.w1;
      if (slot >= capros_GPT_nSlots) {
        COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      node_SetSlot(theGPT, slot, inv);
      return;
    }

  case OC_capros_GPT_setWindow:
    {
      uint64_t offset;
      if (opaque) goto opaqueError;

      uint32_t slot = inv->entry.w1;
      uint32_t baseSlot = inv->entry.w2;
      uint32_t restrictions = inv->entry.w3;

      if (slot >= capros_GPT_nSlots
          || (baseSlot >= capros_GPT_nSlots
              && baseSlot != capros_GPT_windowBaseSlot)
          || (restrictions & ~ (capros_Memory_weak | capros_Memory_noCall
                                | capros_Memory_readOnly))
          || inv->entry.len != sizeof(offset)
         ) {
        COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      inv_CopyIn(inv, inv->entry.len, &offset);

      unsigned int curL2v = gpt_GetL2vField(theGPT) & GPT_L2V_MASK;
      if (offset & ((1ull << curL2v) -1)) {
        COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      node_MakeDirty(theGPT);

      COMMIT_POINT();
  
      node_ClearHazard(theGPT, slot);

      key_SetToNumber(node_GetKeyAtSlot(theGPT, slot),
		      (restrictions << 8) + baseSlot,
		      (uint32_t) (offset >> 32),
		      (uint32_t) offset);

      inv->exit.code = RC_OK;
      return;
    }

  case OC_capros_GPT_clone:
    {
      /* Copy content of GPT in key[0] to current GPT. */
      if (opaque) goto opaqueError;

      key_Prepare(inv->entry.key[0]);

      /* Mark the object dirty. */
      node_MakeDirty(theGPT);

      COMMIT_POINT();

      if (keyBits_GetType(inv->entry.key[0]) != KKT_GPT) {
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      NodeClone(theGPT, inv->entry.key[0]);

      inv->exit.code = RC_OK;
      return;
    }

  case OC_capros_key_getType:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_GPT;
      return;
    }

  case OC_capros_GPT_setKeeper:
    if (opaque) goto opaqueError;

    node_SetSlot(theGPT, capros_GPT_keeperSlot, inv);

    gpt_SetL2vField(theGPT, gpt_GetL2vField(theGPT) | GPT_KEEPER);
    return;

  case OC_capros_GPT_clearKeeper:
    if (opaque) goto opaqueError;

    COMMIT_POINT();

    gpt_SetL2vField(theGPT, gpt_GetL2vField(theGPT) & ~ GPT_KEEPER);
    inv->exit.code = RC_OK;
    return;

  case OC_capros_GPT_setBackground:
    if (opaque) goto opaqueError;

    InvalidateMaps(theGPT);	/* because the background GPT is cached
				in mapping table headers */

    node_SetSlot(theGPT, capros_GPT_backgroundSlot, inv);

    gpt_SetL2vField(theGPT, gpt_GetL2vField(theGPT) | GPT_BACKGROUND);
    return;

  case OC_capros_GPT_clearBackground:
    if (opaque) goto opaqueError;

    InvalidateMaps(theGPT);	/* because the background GPT is cached
				in mapping table headers */

    COMMIT_POINT();

    gpt_SetL2vField(theGPT, gpt_GetL2vField(theGPT) & ~ GPT_BACKGROUND);
    inv->exit.code = RC_OK;
    return;

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }
}
