/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005-2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Node.h>
#include <kerninc/Process.h>
#include <kerninc/Activity.h>
#include <kerninc/IORQ.h>
#include <kerninc/SysTimer.h>
#include <ddb/db_output.h>

#ifdef OPTION_DDB
void
objC_ddb_dump_pinned_objects()
{
  uint32_t userPins = 0;
  uint32_t nd;

  for (nd = 0; nd < objC_nNodes; nd++) {
    Node * pNode = objC_GetCoreNodeFrame(nd);
    ObjectHeader * pObj = node_ToObj(pNode);
    if (objH_IsUserPinned(pObj)) {
      userPins++;
      printf("node %#llx\n", pObj->oid);
    }
  }

  struct CorePageIterator cpi;
  CorePageIterator_Init(&cpi);

  PageHeader * pageH;
  while ((pageH = CorePageIterator_Next(&cpi))) {
    ObjectHeader * pObj = pageH_ToObj(pageH);
    if (objH_IsUserPinned(pObj)) {
      userPins++;
      printf("page %#llx\n", pObj->oid);
    }
  }

  printf("User pins found: %d \n", userPins);
}

static void
objC_ddb_dump_obj(ObjectHeader * pObj)
{
  printf("%#x: %s oid %#llx%s drt:%c%s%s\n",
	 pObj,
	 ddb_obtype_name(pObj->obType),
	 pObj->oid,
	 objH_IsUserPinned(pObj) ? "" : " pinned",
	 objH_IsDirty(pObj) ? 'y' : 'n',
	 objH_IsKRO(pObj) ? "" : " KRO",
         ! objH_isNodeType(pObj) && objH_ToPage(pObj)->ioreq
                ? (objH_GetFlags(pObj, OFLG_Fetching) ? " fetching"
                                                      : " cleaning")
                : "");
}

void
objC_ddb_dump_pages(OID first, OID last)
{
  uint32_t nFree = 0;
  unsigned long notDisplayed = 0;
  
  struct CorePageIterator cpi;
  CorePageIterator_Init(&cpi);

  PageHeader * pageH;
  while ((pageH = CorePageIterator_Next(&cpi))) {

    switch (pageH_GetObType(pageH)) {
    case ot_PtFreeFrame:
    case ot_PtFreeSecondary:
      nFree++;
      break;

    case ot_PtNewAlloc:
      assert(false);	// should not have at this time

    case ot_PtDataPage:
    {
      ObjectHeader * pObj = pageH_ToObj(pageH);
      if (pObj->oid >= first && pObj->oid <= last)
        objC_ddb_dump_obj(pObj);
      else
        notDisplayed++;
      break;
    }

    case ot_PtTagPot:
    case ot_PtHomePot:
    case ot_PtLogPot:
    case ot_PtWorkingCopy:
    {
      ObjectHeader * pObj = pageH_ToObj(pageH);
      if (pObj->oid >= first && pObj->oid <= last)
        printf("%#x: %s oid %#llx\n",
               pObj,
               ddb_obtype_name(pObj->obType),
               pObj->oid);
      else
        notDisplayed++;
      break;
    }

    case ot_PtKernelUse:
    case ot_PtDevBlock:
    case ot_PtDMABlock:
    case ot_PtSecondary:
      if (last >= OID_RESERVED_PHYSRANGE)	// only if "show pages all"
        printf("%#x: %s\n",
               pageH,
               ddb_obtype_name(pageH_GetObType(pageH)) );
      else
        notDisplayed++;
      break;
      
    default:
      if (last >= OID_RESERVED_PHYSRANGE) {	// only if "show pages all"
        printf("%#x: %s ",
               pageH,
               ddb_obtype_name(pageH_GetObType(pageH)) );
        pageH_mdType_dump_pages(pageH);
      } else
        notDisplayed++;
      break;
    }
  }

  printf("Total of %d pages, of which %d are free, %d not displayed\n",
         objC_nPages, nFree, notDisplayed);
}

void
objC_ddb_dump_nodes(OID first, OID last)
{
  uint32_t nFree = 0;
  unsigned long notDisplayed = 0;
  uint32_t nd = 0;
  
  for (nd = 0; nd < objC_nNodes; nd++) {
    ObjectHeader *pObj = node_ToObj(objC_GetCoreNodeFrame(nd));

    if (pObj->obType == ot_NtFreeFrame) {
      nFree++;
      continue;
    }

    if (pObj->obType > ot_NtLAST_NODE_TYPE)
      fatal("Node @0x%08x: object type %d is broken\n", pObj,
		    pObj->obType); 
    if (pObj->oid >= first && pObj->oid <= last)
      objC_ddb_dump_obj(pObj);
    else
      notDisplayed++;
  }

  printf("Total of %d nodes, of which %d are free, %d not displayed\n",
         objC_nNodes, nFree, notDisplayed);
}

void
objC_ddb_dump_procs(void)
{
  uint32_t nFree = 0;
  unsigned int i;
  
  for (i = 0; i < KTUNE_NCONTEXT; i++) {
    Process * proc = &proc_ContextCache[i];
    if (proc->procRoot) {
      printf("%#x oid=%#llx (%s)",
        proc, node_ToObj(proc->procRoot)->oid, proc_Name(proc) );
      if (keyBits_IsType(&proc->procRoot->slot[ProcSymSpace], KKT_Number)) {
        db_printf(" (");
        db_eros_print_number_as_string(&proc->procRoot->slot[ProcSymSpace]);
        db_printf(")");
      }
      if (! (proc->hazards & hz_DomRoot)) {
        switch (proc->runState) {
        default:
          printf(": Runstate=%d!", proc->runState);
          break;

        case RS_Running:
          printf(": Running");
          break;

        case RS_Available:
          printf(": Available");
          break;

        case RS_Waiting:
          printf(": Waiting");
          // Find the resume capability to this process.
          if (! link_isSingleton(&proc->keyRing)) {
            Key * key = (Key *) proc->keyRing.prev;
            if (keyBits_IsPreparedResumeKey(key)) {
              Process * p2 = proc_ValidKeyReg(key);
              if (p2) {
                printf(" on proc %#x", p2);
              }
            }
            // There could be more than one resume key, but we only
            // examine one.
          }
          break;
        }
        if (proc->faultCode) {
          printf(" FaultCode %d", proc->faultCode);
        }
      }
      if (proc->curActivity) {
        Activity * act = proc->curActivity;
        printf(", act=%#x, %s",
               act, act_stateNames[act->state]);
        switch (act->state) {
        case act_Sleeping:
          printf(" for %lld ticks", act->u.wakeTime - sysT_latestTime);
          break;

        case act_Stall: ;
          StallQueue * sq = act->lastq;
          kva_t sqa = (kva_t)sq;
          printf(" q=%#x", sq);
          // Try to identify the queue.
          if (sq == &IOReqWait)
            printf(" (IOReqWait)");
          else if (sq == &IOReqCleaningWait)
            printf(" (IOReqCleaningWait)");
          else if (sqa >= (kva_t)&IORQs[0] && sqa < (kva_t)&IORQs[KTUNE_NIORQS])
            printf(" (IOReq)");
          else if (sqa >= (kva_t)&proc_ContextCache[0]
                   && sqa < (kva_t)&proc_ContextCache[KTUNE_NCONTEXT])
            printf(" (process)");
        default: ;
        }
      }
      printf("\n");
    } else {
      nFree++;
    }
  }

  printf("Total of %d procs, of which %d are free\n", KTUNE_NCONTEXT, nFree);
}
#endif
