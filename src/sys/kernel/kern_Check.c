/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Check.h>
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Node.h>
#include <kerninc/IRQ.h>

#ifdef USES_MAPPING_PAGES
/* This architecture specific. */
bool
check_MappingPage(ObjectHeader *pPage);
#endif

void
check_Consistency(const char *msg)
{
  static bool inCheck = false;

  if (inCheck)
    return;

  inCheck = true;

  check_DoConsistency(msg);

  inCheck = false;
}

extern uint32_t InterruptStackLimit;
extern uint32_t InterruptStackTop;

void
check_DoConsistency(const char *msg)
{
  /* FIX: I'm not sure what should be replacing the TrapDepth check
   * here. The idea is to be checking whether we are in the kernel or
   * not. */
#if defined(DBG_WILD_PTR) && 0
  if (TrapDepth == 0) {
    assert ( Thread::Current()->IsKernel() );

    KernThread* kt = (KernThread *) Thread::Current();

    if ((uint32_t) &msg < (uint32_t) kt->procContext.stackBottom)
      fatal("Kernel stack overflow\n");
  
    if ((uint32_t) &msg >= (uint32_t) kt->procContext.stackTop)
      fatal("Kernel stack underflow\n");
  }
  else {  
    if ((uint32_t) &msg < (uint32_t) &InterruptStackLimit)
      fatal("User stack overflow\n");
  
    if ((uint32_t) &msg >= (uint32_t) &InterruptStackTop)
      fatal("User stack underflow\n");
  }
#endif
  
  static const char *lastMsg = "(unknown)";
  if ( !check_Pages()
       || !check_Nodes()
       || !check_Contexts("from checkcnsist")
       )
    fatal("ChkConsistency() fails:\n"
		  "\twhere: %s\n"
		  "\tlast:  %s\n",
		  msg, lastMsg);
  lastMsg = msg;
}

bool
check_Nodes()
{
  uint32_t nd = 0;
  bool result = true;
  
  irq_DISABLE();

  for (nd = 0; nd < objC_NumCoreNodeFrames(); nd++) {
    /* printf("CheckNode(%d)\n", frame); */

    Node *pNode = objC_GetCoreNodeFrame(nd);

    if (node_Validate(pNode) == false)
      result = false;

    if (result == false)
      break;
  }

  irq_ENABLE();

  return result;
}

bool
check_Pages()
{
  uint32_t pg = 0;
#ifndef NDEBUG
  uint32_t chk = 0;
#endif
  bool result = true;

  irq_DISABLE();

  for (pg = 0; pg < objC_NumCorePageFrames(); pg++) {
    /*  printf("CheckPage(%d)\n", frame); */

    ObjectHeader *pPage = objC_GetCorePageFrame(pg);

    assert(pPage->obType > ot_NtLAST_NODE_TYPE);

    if (pPage->obType == ot_PtFreeFrame || pPage->obType == ot_PtNewAlloc)
      continue;

#ifndef NDEBUG
    if ( !keyR_IsValid(&pPage->keyRing, pPage) ) {
      result = false;
      break;
    }
#endif

#ifdef OPTION_OB_MOD_CHECK

    if (!objH_GetFlags(pPage, OFLG_DIRTY)) {
      assert (objH_GetFlags(pPage, OFLG_DIRTY) == 0);
      if (objH_GetFlags(pPage, OFLG_REDIRTY))
	printf("Frame %d ty=%d, flg=0x%02x redirty but not dirty!!\n",
		       pg, pPage->obType, pPage->flags);

      assert (objH_GetFlags(pPage, OFLG_DIRTY) == 0);
      chk = objH_CalcCheck(pPage);

      if (objH_GetFlags(pPage, OFLG_DIRTY) != 0)
	printf("Frame %d Chk=0x%x CalcCheck=0x%x flgs=0x%02x ty=%d on pgHdr 0x%08x\n",
		       pg, pPage->kt_u.ob.check, chk, pPage->flags,
		       pPage->obType, pPage);

      
      assert (objH_GetFlags(pPage, OFLG_DIRTY) == 0);
  
      if ( pPage->kt_u.ob.check != chk ) {
	printf("Frame %d Chk=0x%x CalcCheck=0x%x flgs=0x%02x ty=%d on pg ",
		       pg, pPage->kt_u.ob.check, chk, pPage->flags, pPage->obType);
	printOid(pPage->kt_u.ob.oid);
	printf("\n");
	printf("  pPage 0x%08x dirty: %c reDirty: %c\n",
		       pPage,
		       (objH_GetFlags(pPage, OFLG_DIRTY) ? 'y' : 'n'),
		       (objH_GetFlags(pPage, OFLG_REDIRTY) ? 'y' : 'n'));
	result = false;
	break;
      }
    }

#endif

#ifdef USES_MAPPING_PAGES
    if (pPage->obType == ot_PtMappingPage)
      if ( check_MappingPage(pPage) == false ) {
	result = false;
	break;
      }
#endif
  }

  irq_ENABLE();

  return result;
}
