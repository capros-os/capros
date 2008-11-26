/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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
#include <kerninc/Check.h>
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Node.h>
#include <kerninc/IRQ.h>
#include <kerninc/ObjH-inline.h>

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
  
  irqFlags_t flags = local_irq_save();

  for (nd = 0; nd < objC_NumCoreNodeFrames(); nd++) {
    /* printf("CheckNode(%d)\n", frame); */

    Node *pNode = objC_GetCoreNodeFrame(nd);

    if (node_Validate(pNode) == false)
      result = false;

    if (result == false)
      break;
  }

  local_irq_restore(flags);

  return result;
}

// Returns false if an error.
bool
check_Pages()
{
  int i;
  uint32_t pg;
  bool result = true;

  irqFlags_t flags = local_irq_save();

  for (pg = 0; pg < objC_NumCorePageFrames(); pg++) {
    /*  printf("CheckPage(%d)\n", frame); */

    PageHeader * pPage = objC_GetCorePageFrame(pg);

    switch (pageH_GetObType(pPage)) {
    case ot_PtFreeFrame:
    {
      unsigned int nPages = 1U << pPage->kt_u.free.log2Pages;
      for (i = nPages - 1; i > 0; i--) {
        PageHeader * pPage2 = objC_GetCorePageFrame(++pg);
        if (pageH_GetObType(pPage2) != ot_PtSecondary) {
          printf("Frame %#x free but %#x not secondary\n", pPage, pPage2);
          result = false;
          goto exitLoop;
        }
      }
      continue;
    }

    case ot_PtNewAlloc:
    case ot_PtKernelUse:
    case ot_PtDMABlock:
    case ot_PtDMASecondary:
    case ot_PtTagPot:
    case ot_PtHomePot:
    case ot_PtLogPot:
      continue;

    case ot_PtWorkingCopy:
      if (! objH_GetFlags(pageH_ToObj(pPage), OFLG_KRO)) {
        printf("pageH %#x wkg copy but not KRO\n", pPage);
        result = false;
      }
      continue;

    case ot_PtDevicePage:
    case ot_PtDataPage:
#ifndef NDEBUG
      if ( !keyR_IsValid(&pageH_ToObj(pPage)->keyRing, pPage) ) {
        result = false;
        break;
      }
#endif

#ifdef OPTION_OB_MOD_CHECK
      ObjectHeader * pObj = pageH_ToObj(pPage);
      if (objH_IsUnwriteable(pObj)) {
        uint32_t chk = pageH_CalcCheck(pPage);

        if (pObj->check != chk) {
          printf("Frame %d Chk=0x%x CalcCheck=0x%x flgs=0x%02x ty=%d\n on pg OID ",
                 pg, pObj->check, chk, pObj->flags, pObj->obType);
	  printOid(pObj->oid);
	  printf("  pPage 0x%08x dirty\n",
		       pPage);
	  result = false;
        }
      }
#endif
      break;

    default:
      if (! pageH_mdType_CheckPage(pPage)) {
	result = false;
      }
      break;
    }

    if (! result) break;	// no point continuing the loop
  }
exitLoop:

  local_irq_restore(flags);

  return result;
}
