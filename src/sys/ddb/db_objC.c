/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, Strawberry Development Group.
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
  uint32_t nd, pg;

  for (nd = 0; nd < objC_nNodes; nd++) {
    Node * pNode = objC_GetCoreNodeFrame(nd);
    ObjectHeader * pObj = node_ToObj(pNode);
    if (objH_IsUserPinned(pObj)) {
      userPins++;
      printf("node %#llx\n", pObj->oid);
    }
  }

  for (pg = 0; pg < objC_nPages; pg++) {
    PageHeader * pageH = objC_GetCorePageFrame(pg);
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
  char goodSum;
#ifdef OPTION_OB_MOD_CHECK
  goodSum = (pObj->check == objH_CalcCheck(pObj)) ? 'y' : 'n';
#else
  goodSum = '?';
#endif
  printf("%#x: %s oid %#llx up:%c drt:%c kro:%c sm:%c au:%c cu:%c\n",
	 pObj,
	 ddb_obtype_name(pObj->obType),
	 pObj->oid,
	 objH_IsUserPinned(pObj) ? 'y' : 'n',
	 objH_IsDirty(pObj) ? 'y' : 'n',
	 objH_IsKRO(pObj) ? 'y' : 'n',
	 goodSum,
	 objH_GetFlags(pObj, OFLG_AllocCntUsed) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_CallCntUsed) ? 'y' : 'n');
}

void
objC_ddb_dump_pages()
{
  uint32_t nFree = 0;
  uint32_t pg;
  
  for (pg = 0; pg < objC_nPages; pg++) {
    PageHeader * pageH = objC_GetCorePageFrame(pg);

    switch (pageH_GetObType(pageH)) {
    case ot_PtFreeFrame:
    case ot_PtSecondary:
      nFree++;
      break;

    case ot_PtNewAlloc:
      assert(false);	// should not have at this time

    case ot_PtDataPage:
    case ot_PtDevicePage:
    {
      ObjectHeader * pObj = pageH_ToObj(pageH);
      objC_ddb_dump_obj(pObj);
      break;
    }

    case ot_PtTagPot:
    case ot_PtHomePot:
    case ot_PtLogPot:
    case ot_PtWorkingCopy:
    {
      ObjectHeader * pObj = pageH_ToObj(pageH);
      printf("%#x(%d): %s oid %#llx\n",
	 pObj, pg,
	 ddb_obtype_name(pObj->obType),
	 pObj->oid);
      break;
    }

    case ot_PtKernelUse:
    case ot_PtDMABlock:
    case ot_PtDMASecondary:
      printf("%#x: %s\n",
             pageH,
             ddb_obtype_name(pageH_GetObType(pageH)) );
      break;
      
    default:
      printf("%#x: %s ",
             pageH,
             ddb_obtype_name(pageH_GetObType(pageH)) );
      pageH_mdType_dump_pages(pageH);
      break;
    }
  }

  printf("Total of %d pages, of which %d are free\n", objC_nPages, nFree);
}

void
objC_ddb_dump_nodes()
{
  uint32_t nFree = 0;
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
    objC_ddb_dump_obj(pObj);
  }

  printf("Total of %d nodes, of which %d are free\n", objC_nNodes, nFree);
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
