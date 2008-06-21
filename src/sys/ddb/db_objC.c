/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

#ifdef OPTION_DDB
void
objC_ddb_dump_pinned_objects()
{
  extern void db_printf(const char *fmt, ...);
  uint32_t userPins = 0;
  uint32_t nd, pg;

  for (nd = 0; nd < objC_nNodes; nd++) {
    Node * pNode = objC_GetCoreNodeFrame(nd);
    ObjectHeader * pObj = node_ToObj(pNode);
    if (objH_IsUserPinned(pObj) || node_IsKernelPinned(pNode)) {
      if (objH_IsUserPinned(pObj))
	userPins++;
      printf("node 0x%08x%08x\n",
	     (uint32_t) (pObj->oid >> 32),
	     (uint32_t) pObj->oid);
    }
  }

  for (pg = 0; pg < objC_nPages; pg++) {
    PageHeader * pageH = objC_GetCorePageFrame(pg);
    ObjectHeader * pObj = pageH_ToObj(pageH);
    if (objH_IsUserPinned(pObj) || pageH_IsKernelPinned(pageH)) {
      if (objH_IsUserPinned(pObj))
	userPins++;
      printf("page 0x%08x%08x\n",
	     (uint32_t) (pObj->oid >> 32),
	     (uint32_t) pObj->oid);
    }
  }

#ifdef OLD_PIN
  printf("User pins found: %d official count: %d\n", userPins,
	 ObjectHeader::PinnedObjectCount);
#else
  printf("User pins found: %d \n", userPins);
#endif
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
  printf("%#x: %s oid %#llx up:%c cr:%c ck:%c drt:%c io:%c sm:%c au:%c cu:%c\n",
	 pObj,
	 ddb_obtype_name(pObj->obType),
	 pObj->oid,
	 objH_IsUserPinned(pObj) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_CURRENT) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_CKPT) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_DIRTY) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_IO) ? 'y' : 'n',
	 goodSum,
	 objH_GetFlags(pObj, OFLG_AllocCntUsed) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_CallCntUsed) ? 'y' : 'n');
}

void
objC_ddb_dump_pages()
{
  uint32_t nFree = 0;
  uint32_t pg;
  
  extern void db_printf(const char *fmt, ...);

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
    {
      ObjectHeader * pObj = pageH_ToObj(pageH);
      printf("%#x: %s oid %#llx\n",
	 pObj,
	 ddb_obtype_name(pObj->obType),
	 pObj->oid);
      break;
    }

    case ot_PtKernelHeap:
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
  
  extern void db_printf(const char *fmt, ...);

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
#endif
