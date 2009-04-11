/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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
#include <arch-kerninc/db_machdep.h>
#include <ddb/db_output.h>
#include <ddb/db_lex.h>
#include <ddb/db_command.h>

#include <kerninc/Machine.h>
#include <kerninc/Check.h>
#include <kerninc/Invocation.h>
#include <kerninc/Process.h>
#include <arch-kerninc/Process-inline.h>
#include <kerninc/GPT.h>
#include <kerninc/Activity.h>
#include <kerninc/util.h>
#include <kerninc/KernStream.h>
#include <kerninc/SymNames.h>
#include <kerninc/IRQ.h>
#include <kerninc/SysTimer.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Depend.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/PhysMem.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/KernStats.h>
#include <kerninc/ObjH-inline.h>
#include <kerninc/Node-inline.h>
#include <eros/Reserve.h>
#include <eros/Invoke.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <arch-kerninc/PTE.h>

bool continue_user_bpt;

static void
db_print_keyflags(Key * key)
{
  db_printf(" %c%c%c",
    key->keyFlags & KFL_PREPARED ? 'P' : '.',
    key->keyFlags & KFL_RHAZARD ? 'R' : '.',
    key->keyFlags & KFL_WHAZARD ? 'W' : '.' );
}

static void
db_print_gate(Key * key, const char * name)
{
  db_printf(name);

  db_print_keyflags(key);

  uint32_t *pWKey = (uint32_t *) key;

  if (keyBits_IsPreparedObjectKey(key)) {
    Process * proc = key->u.gk.pContext;
    ObjectHeader * pObj = node_ToObj(proc->procRoot);
    
    db_printf(" 0x%08x proc=%#x (oid=%#llx root=0x%08x)\n",
		pWKey[3], proc, pObj->oid, pObj);
  } else {
    db_printf(" 0x%08x cnt=%#x oid=%#llx\n",
	      pWKey[3], key->u.unprep.count, key->u.unprep.oid);
  }
}

static void
db_print_node_key(Key * key, const char * name)
{
  db_printf(name);

  db_print_keyflags(key);

  uint32_t *pWKey = (uint32_t *) key;

  if (keyBits_IsPreparedObjectKey(key)) {
    ObjectHeader * pObj = key->u.ok.pObj;
    
    db_printf(" 0x%08x objh=%#x (oid=%#llx)\n",
		pWKey[3], pObj, pObj->oid);
  } else {
    db_printf(" 0x%08x cnt=%#x oid=%#llx\n",
	      pWKey[3], key->u.unprep.count, key->u.unprep.oid);
  }
}

static void
db_print_proc_key(Key * key, const char * name)
{
  db_printf(name);

  db_print_keyflags(key);

  if (keyBits_IsPreparedObjectKey(key)) {
    Process * proc = key->u.gk.pContext;
    ObjectHeader * pObj = node_ToObj(proc->procRoot);
    
    db_printf(" proc=%#x (oid=%#llx root=0x%08x)\n",
		proc, pObj->oid, pObj);
  } else {
    db_printf(" cnt=%#x oid=%#llx\n",
	      key->u.unprep.count, key->u.unprep.oid);
  }
}

static void
db_print_mem_key(Key * key, const char * name)
{
  db_printf(name);

  db_print_keyflags(key);

  db_printf(" %c%c%c%c",
    key->keyPerms & capros_Memory_readOnly ? 'R' : '.',
    key->keyPerms & capros_Memory_noCall ? 'N' : '.',
    key->keyPerms & capros_Memory_weak ? 'W' : '.',
    key->keyPerms & capros_Memory_opaque ? 'O' : '.' );

  uint64_t guard = key_GetGuard(key);
  if (guard)
    db_printf(" guard=%#llx", guard);

  if (keyBits_IsPreparedObjectKey(key)) {
    ObjectHeader * pObj = key->u.ok.pObj;
    
    db_printf(" objh=%#x (oid=%#llx)\n",
		pObj, pObj->oid);
  } else {
    db_printf(" cnt=%#x oid=%#llx\n",
	      key->u.unprep.count, key->u.unprep.oid);
  }
}

void
db_eros_print_key(Key* key /*@ not null @*/)
{
  uint32_t *pWKey = (uint32_t *) key;
  KeyType kt = keyBits_GetType(key);

  switch (kt) {
  case KKT_Resume:
    db_print_gate(key, "rsum");
    break;
  case KKT_Start:
    db_print_gate(key, "strt");
    break;
  case KKT_Forwarder:
    db_print_node_key(key, "fwdr");
    break;
  case KKT_Node:
    db_print_node_key(key, "node");
    break;
  case KKT_GPT:
    db_print_mem_key(key, "GPT ");
    break;
  case KKT_Process:
    db_print_proc_key(key, "proc");
    break;
  case KKT_Page:
    db_print_mem_key(key, "page");
    break;
  default:
    assert(! keyBits_IsObjectKey(key));
    switch (kt) {
    case KKT_Void:
      db_printf("void");
      db_print_keyflags(key);
      db_printf("\n");
      break;
    case KKT_Number:
      db_printf("num ");
      db_print_keyflags(key);
      db_printf(" 0x%08x 0x%08x 0x%08x\n",
	        pWKey[0], pWKey[1], pWKey[2]);
      break;
    default:
      db_printf("t=%02d", keyBits_GetType(key));
      db_print_keyflags(key);
      db_printf(" 0x%08x 0x%08x 0x%08x 0x%08x\n",
	        pWKey[0], pWKey[1], pWKey[2], pWKey[3]);
    }
  }
}

#define __EROS_PRIMARY_KEYDEF(name, isValid, bindTo)  #name,
static const char *keyNames[] = {
#include <eros/StdKeyType.h>
};

void
db_eros_print_key_details(Key* key /*@ not null @*/)
{
  db_printf("Key (0x%08x) (KKT_%s) keydata=0x%x flags=",
	    key, keyNames[keyBits_GetType(key)],  key->keyData);

  if (keyBits_IsPrepared(key) || keyBits_IsHazard(key)) {
    db_printf("(");

    if (keyBits_IsRdHazard(key)) 
      db_printf("rdhz");
    if (keyBits_IsWrHazard(key)) {
      if (keyBits_IsRdHazard(key)) 
	db_printf(", ");
      db_printf("wrhz");
    }
    if (keyBits_IsPrepared(key)) {
      if (keyBits_IsHazard(key)) 
	db_printf(", ");
      db_printf("prepared");
    }
    db_printf(")");
  }
  db_printf("\n");

  if (keyBits_IsObjectKey(key)) {
    OID oid;
    ObCount count;
 
    if (keyBits_IsUnprepared(key)) {
      oid = key->u.unprep.oid;
      count = key->u.unprep.count;
    }
    else if (keyBits_IsType(key, KKT_Resume)) {
      oid = key->u.gk.pContext->procRoot->node_ObjHdr.oid;
      count = node_GetCallCount(key->u.gk.pContext->procRoot);
    }
    else if (keyBits_IsType(key, KKT_Start)) {
      oid = key->u.gk.pContext->procRoot->node_ObjHdr.oid;
      count = objH_GetAllocCount(& key->u.gk.pContext->procRoot->node_ObjHdr);
    }
    else {
      oid = key_GetKeyOid(key);
      count = key_GetAllocCount(key);
    }

    db_printf("oid=%#llx, count=%d", oid, count);

    if (keyBits_IsSegModeType(key)) {
      if (keyBits_IsReadOnly(key))
	db_printf(" ro");
      if (keyBits_IsWeak(key))
	db_printf(" wk");
      if (keyBits_IsNoCall(key))
	db_printf(" nc");
    }
    db_printf("\n");
  }
  else if (keyBits_IsType(key, KKT_Range) || keyBits_IsType(key, KKT_PrimeRange) ||
	   keyBits_IsType(key, KKT_PhysRange)) {
    OID start = key->u.rk.oid;
    OID end = key->u.rk.oid + inv.key->u.rk.count;

    db_printf("oid=[%#llx, %#llx)\n", start, end);
  }
  else if (keyBits_IsType(key, KKT_Number)) {
    db_printf("0x%08x%08x%08x\n",
	      key->u.nk.value[2],
	      key->u.nk.value[1],
	      key->u.nk.value[0]);
  }
  else if (keyBits_IsVoidKey(key)) {
  }
  else {
    db_printf("0x%08x 0x%08x 0x%08x\n",
	      key->u.nk.value[0],
	      key->u.nk.value[1],
	      key->u.nk.value[2]);
  }
}

void
db_eros_print_node(Node *pNode)
{
  uint32_t i;

  db_printf("Node (0x%08x) oid=%#llx ac=0x%08x cc=0x%08x ot=%d data=0x%x\n",
	    pNode, pNode->node_ObjHdr.oid,
	    objH_GetAllocCount(& pNode->node_ObjHdr),
	    node_GetCallCount(pNode),
            pNode->node_ObjHdr.obType, pNode->nodeData);

#if !defined(NDEBUG) && 0
  if (! pNode->Validate())
    db_error("...Is not valid\n");
#endif
  
  for (i = 0; i < EROS_NODE_SIZE; i++) {
    Key* key /*@ not null @*/ = node_GetKeyAtSlot(pNode, i);
    db_printf(" [%02d] (0x%08x) ", i, key);
    db_eros_print_key(key);
  }
}

void
db_eros_print_string(uint8_t *data, uint32_t len)
{
  uint32_t i = 0;

  if ( len > 20)
    len = 20;
  
  for (i = 0; i < len; i++) {
    uint8_t c = data[i];
  
    if (kstream_IsPrint(c) || c == '\n')
      db_printf("%c", c);
    else if (c == 0)	// terminate on NUL
      break;
    else
      db_printf("\\x%02x", c);
  }
}

void
db_eros_print_number_as_string(Key* k /*@ not null @*/)
{
  unsigned ndx;
  uint8_t theName[12];

  for (ndx = 0; ndx < 12; ndx++)
    theName[ndx] = k->u.nk.value[ndx / 4] >> (8 * (ndx % 4));

  db_eros_print_string(theName, 12);
}

void
db_eros_print_context(Process *cc)
{
    db_printf("proc=0x%08x (%s)", cc, proc_Name(cc));
    const char * stateName;
    switch (cc->runState) {
    default: stateName = "?"; break;
    case RS_Available: stateName = "Avail"; break;
    case RS_Running: stateName = "Running"; break;
    case RS_Waiting: stateName = "Waiting"; break;
    }
    db_printf(" (%s) ", stateName);
    db_printf("act=%#x ", cc->curActivity);
    if (cc->isUserContext) {
      db_printf("root=%#x  ", cc->procRoot);

      if (cc->procRoot) {
	db_printf("oid=%#llx\n", cc->procRoot->node_ObjHdr.oid);
      }
      else
	db_printf("\n");
    } else {
      db_printf("(kernel)\n");
    }

    db_printf("  Fault Code=%u Fault Info=%#x procFlags=0x%02x\n"
	      "  haz=0x%x",
	      cc->faultCode, cc->faultInfo, cc->processFlags,
	      cc->hazards);
    if (! (cc->hazards & hz_Schedule))
      db_printf(" prio=%d", /*cc->priority*/cc->readyQ->mask);
    db_eros_print_context_md(cc);
 
    if (cc->procRoot && 
	keyBits_IsType(&cc->procRoot->slot[ProcSymSpace], KKT_Number)) {
      db_printf("Procname: ");
      db_eros_print_number_as_string(&cc->procRoot->slot[ProcSymSpace]);
      db_printf("\n");
    }

    if (proc_IsNotRunnable(cc))
      printf("Note: process is NOT runnable\n");
    proc_DumpFixRegs(cc);
#ifdef OPTION_PSEUDO_REGS
    proc_DumpPseudoRegs(cc);
#endif
}

extern void DumpFixRegs(const savearea_t *fx);

void
db_show_savearea_cmd(db_expr_t addr, int have_addr,
		     db_expr_t cnt/* count */, char * mdf/* modif */)
{

  
  if (have_addr == 0)
    db_error("requires address\n");

  DumpFixRegs((const savearea_t *) addr);
}

void
db_show_sizes_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		     db_expr_t cnt/* count */, char * mdf/* modif */)
{
  db_printf("sizeof(ObjectHeader) = 0x%x (%d)\n",
	    sizeof(ObjectHeader), sizeof(ObjectHeader));
  db_printf("sizeof(PageHeader) = 0x%x (%d), num=%d\n",
            sizeof(PageHeader), sizeof(PageHeader), objC_nPages);
  db_printf("sizeof(Node) = 0x%x (%d), num=%d\n",
	    sizeof(Node), sizeof(Node), objC_NumCoreNodeFrames());
  db_printf("sizeof(Process) = %#x (%d), num=%d\n",
	    sizeof(Process), sizeof(Process), KTUNE_NCONTEXT);
  db_printf("sizeof(Activity) = %#x (%d), num=%d\n",
	    sizeof(Activity), sizeof(Activity), KTUNE_NACTIVITY);
  db_printf("sizeof(log dir entry) = %#x (%d), num=%d\n",
	    ld_getDirEntrySize(), ld_getDirEntrySize(), numLogDirEntries);
  db_printf("space for each Depend bucket = %#x (%d), num=%d\n",
            Depend_getSize(), Depend_getSize(), Depend_getNumBuckets());
}


void
db_eros_print_keyring(KeyRing * kr)
{
  Link * l;
  for (l = kr->next; l != kr; l = l->next) {
    db_printf("(%#x): ", l);
    // The following works for process keys as well as object keys.
    Key * k = container_of(l, Key, u.ok.kr);
    switch (key_ValidKeyPtr(k)) {
    case 1:
    {
      Node * node = node_ValidNodeKeyPtr(k);
      db_printf("(in node %#x slot %d): ", node, k - node->slot);
      break;
    }

    case 2:
    {
      Process * proc = proc_ValidKeyReg(k);
      db_printf("(in Process %#x keyreg %d): ", proc, k - proc->keyReg);
      break;
    }

    default:
      break;
    }
    db_eros_print_key(k);
  }
}

void
db_eros_print_context_keyregs(Process *cc)
{
  unsigned int i = 0;

  if (cc == 0) {
    db_printf("invokee=0x%08x\n", cc);
    return;
  }

  for (i = 0; i < EROS_NODE_SIZE; i++) {
    db_printf("[%02d] (0x%08x)", i, &cc->keyReg[i]);
    db_eros_print_key(&cc->keyReg[i]);
  }
}

void
db_eros_print_activity(Activity * act)
{
  db_printf("%#x: %s q=%#x lnkd? %c readyQ %#x readyMask %#x\n",
	    act,
	    act_stateNames[act->state],
	    act->lastq,
	    (act->q_link.prev == &act->q_link) ? 'n' : 'y',
	    act->readyQ,
            act->readyQ->mask);
  
  if (act_IsUser(act)) {
    if (act_HasProcess(act)) {
      Process * proc = act_GetProcess(act);
      db_printf("  proc=%#x (oid=%#llx)\n",
               proc, node_ToObj(proc->procRoot)->oid);
    } else {
      db_printf("  oid=%#llx\n", act->id.oid);
    }
  } else {	// kernel Activity
    db_printf("  (kernel)\n");
  }

  if (act->state == act_Sleeping) {
    db_printf("  sleeping for %d ticks\n",
              act->u.wakeTime - sysT_Now() );
  }
}

#ifdef DBG_WILD_PTR
unsigned dbg_wild_ptr = true;
void
db_eros_dbg_wild_n_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  dbg_wild_ptr = false;
}

void
db_eros_dbg_wild_y_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  dbg_wild_ptr = true;
}
#endif

#ifndef NDEBUG
unsigned dbg_inttrap = false;
void
db_eros_dbg_inttrap_n_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  dbg_inttrap = false;
}

void
db_eros_dbg_inttrap_y_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  dbg_inttrap = true;
}
#endif

bool ddb_activity_uqueue_debug = false;
void
db_eros_mesg_uqueue_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (ddb_activity_uqueue_debug) {
    ddb_activity_uqueue_debug = false;
    db_printf("ddb will NOT stop on user activity queueing\n");
  }
  else {
    ddb_activity_uqueue_debug = true;
    db_printf("ddb will stop on user activity queueing\n");
  }
}

bool ddb_segwalk_debug = false;
void
db_eros_mesg_segwalk_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (ddb_segwalk_debug) {
    ddb_segwalk_debug = false;
    db_printf("ddb will NOT stop in segment traversal\n");
  }
  else {
    ddb_segwalk_debug = true;
    db_printf("ddb will stop in segment traversal\n");
  }
}


bool ddb_uyield_debug = false;
void
db_eros_mesg_uyield_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (ddb_uyield_debug) {
    ddb_uyield_debug = false;
    db_printf("ddb will NOT stop on user activity yield\n");
  }
  else {
    ddb_uyield_debug = true;
    db_printf("ddb will stop on user activity yield\n");
  }
}

void
db_eros_mesg_gate_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (ddb_inv_flags & DDB_INV_gate) {
    ddb_inv_flags &= ~DDB_INV_gate;
    db_printf("ddb will NOT stop on gate key invocations\n");
  }
  else {
    ddb_inv_flags |= DDB_INV_gate;
    db_printf("ddb will stop on gate key invocations\n");
  }
}

void
db_eros_mesg_return_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
			db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (ddb_inv_flags & DDB_INV_return) {
    ddb_inv_flags &= ~DDB_INV_return;
    db_printf("ddb will NOT stop on return invocations\n");
  }
  else {
    ddb_inv_flags |= DDB_INV_return;
    db_printf("ddb will stop on return invocations\n");
  }
}

void
db_eros_mesg_allinv_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
			db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (ddb_inv_flags & DDB_INV_all) {
    ddb_inv_flags &= ~DDB_INV_all;
    db_printf("ddb will NOT stop on all invocations\n");
  }
  else {
    ddb_inv_flags |= DDB_INV_all;
    db_printf("ddb will stop on all invocations\n");
  }
}

void
db_eros_mesg_procinv_cmd(db_expr_t addr, int have_addr,
			db_expr_t cnt/* count */, char * mdf/* modif */)
{
  Process *cc =
    have_addr ? (Process *) addr : act_CurContext();
  
  if (cc->kernelFlags & KF_DDBINV) {
    cc->kernelFlags &= ~KF_DDBINV;
    db_printf("Invocation traps for process 0x%08x (OID %#llx) disabled\n",
	      cc, 
	      cc->procRoot->node_ObjHdr.oid);  
  }
  else {
    cc->kernelFlags |= KF_DDBINV;
    db_printf("Invocation traps for process 0x%08x (OID %#llx) enabled\n",
	      cc, 
	      cc->procRoot->node_ObjHdr.oid);  
 
    /* Once set, this is forever: */
    ddb_inv_flags |= DDB_INV_pflag;
  }
}

void
db_eros_mesg_proctrap_cmd(db_expr_t addr, int have_addr,
			db_expr_t cnt/* count */, char * mdf/* modif */)
{
  Process *cc =
    have_addr ? (Process *) addr : act_CurContext();
 
  if (cc->kernelFlags & KF_DDBTRAP) {
    cc->kernelFlags &= ~KF_DDBTRAP;
    db_printf("Exception traps for process 0x%08x (OID %#llx) disabled\n",
	      cc, 
	      cc->procRoot->node_ObjHdr.oid);
  }
  else {
    cc->kernelFlags |= KF_DDBTRAP;
    db_printf("Exception traps for process 0x%08x (OID %#llx) enabled\n",
	      cc, 
	      cc->procRoot->node_ObjHdr.oid);
 
    /* Once set, this is forever: */
    ddb_inv_flags |= DDB_INV_pflag;
  }
}

void
db_eros_mesg_keyerr_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (ddb_inv_flags & DDB_INV_keyerr) {
    ddb_inv_flags &= ~DDB_INV_keyerr;
    db_printf("ddb will NOT stop on key errors\n");
  }
  else {
    ddb_inv_flags |= DDB_INV_keyerr;
    db_printf("ddb will stop on key errors\n");
  }
}

void
db_eros_mesg_keeper_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (ddb_inv_flags & DDB_INV_keeper) {
    ddb_inv_flags &= ~DDB_INV_keeper;
    db_printf("ddb will NOT stop on keeper invocations\n");
  }
  else {
    ddb_inv_flags |= DDB_INV_keeper;
    db_printf("ddb will stop on keeper invocations\n");
  }
}

void
db_eros_mesg_show_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  db_printf("Stopping on");
  if(ddb_segwalk_debug)
    db_printf(" segwalk");
  if (ddb_activity_uqueue_debug)
    db_printf(" uqueue");
  if (ddb_uyield_debug)
    db_printf(" uyield");
  if (ddb_inv_flags & DDB_INV_gate)
    db_printf(" gate");
  if (ddb_inv_flags & DDB_INV_keeper)
    db_printf(" keeper");
  if (ddb_inv_flags & DDB_INV_keyerr)
    db_printf(" keyerr");
  if (ddb_inv_flags & DDB_INV_all)
    db_printf(" allinv");
  if (ddb_inv_flags & DDB_INV_return)
    db_printf(" return");
  if (ddb_inv_flags & DDB_INV_pflag)
    db_printf(" process-inv-or-trap");
  db_printf("\n");
}

void
db_ctxt_print_cmd(db_expr_t addr, int have_addr,
		  db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (have_addr)
    db_eros_print_context((Process *) addr);
  else {
    Process * proc = proc_curProcess;
    if (proc)
      db_eros_print_context(proc);
    else
      db_printf("No current process.\n");
  }
}


extern void db_continue_cmd(db_expr_t, int, db_expr_t, char*);
void
db_user_single_step_cmd(db_expr_t addr, int have_addr,
			db_expr_t cnt/* count */, char * mdf/* modif */)
{
  Process *cc =
    have_addr ? (Process *) addr : act_CurContext();

  proc_SetInstrSingleStep(cc);

  {
    
    db_continue_cmd(0, 0, 0, "");
  }
}

#if 0
void
db_print_reserve(CpuReserve *r)
{
  int index = -1;
  
  if ((uint32_t)r > (uint32_t) cpuR_CpuReserveTable)
    index = r - cpuR_CpuReserveTable;
  if (index >= MAX_CPU_RESERVE)
    index = -1;
  
  db_printf("[%3d] (0x%08x) period %#llx duration %#llx\n"
	    "      quanta %#llx normPrio %d rsrvPrio %d\n"
	    "      residQ %#llx residD %#llx expired? %c\n",
	    index, r, r->period, r->duration,
	    r->quanta, r->normPrio, r->rsrvPrio,
	    r->residQuanta, r->residDuration,
	    r->expired ? 'y' : 'n');
}	      

void
db_show_reserves_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		     db_expr_t cnt/* count */, char * mdf/* modif */)
{
  uint32_t i = 0;

  for (i = 0; i < MAX_CPU_RESERVE; i++) {
    CpuReserve* r /*@ not null @*/ = &cpuR_CpuReserveTable[i];
    if (r->activityChain.next == 0)
      continue;

    db_print_reserve(r);
  }
}

void
db_show_kreserves_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		     db_expr_t cnt/* count */, char * mdf/* modif */)
{
  db_print_reserve(&cpuR_KernIdleCpuReserve);
  db_print_reserve(&cpuR_KernActivityCpuReserve);
}

void
db_rsrv_print_cmd(db_expr_t addr, int have_addr,
		  db_expr_t cnt/* count */, char * mdf/* modif */)
{
  CpuReserve *pCpuReserve = cpuR_Current;
  
  if (have_addr)
    pCpuReserve = (CpuReserve *) addr;

  if (pCpuReserve)
    db_print_reserve(pCpuReserve);
  else
    db_error("no current reserve\n");
}

void
db_rsrvchain_print_cmd(db_expr_t addr, int have_addr,
		  db_expr_t cnt/* count */, char * mdf/* modif */)
{
  CpuReserve *pCpuReserve = cpuR_Current;
  Link *tlnk = 0;
  
  if (have_addr)
    pCpuReserve = (CpuReserve *) addr;

  db_print_reserve(pCpuReserve);

  tlnk = pCpuReserve->activityChain.next;
  while (tlnk) {
    Activity *t = act_ActivityFromCpuReserveLinkage(tlnk);
    db_eros_print_activity(t);
    tlnk = tlnk->next;
  }
}
#endif

static void
showDepEntry(KeyDependEntry * kde)
{
  db_printf("(%#x): cnt %d at %#x\n",
            kde, kde->pteCount, kde->start);
}

void
db_show_depend_cmd(db_expr_t addr, int have_addr, db_expr_t cnt, char * mdf)
{
  if (have_addr == 0)
    db_error("requires address\n");

  Depend_VisitEntries((Key *)addr, &showDepEntry);
}

void
db_show_keyring_cmd(db_expr_t addr, int have_addr, db_expr_t cnt, char * mdf)
{
  if (have_addr == 0)
    db_error("requires address\n");

  db_eros_print_keyring((KeyRing *)addr);
}

void
db_ctxt_kr_print_cmd(db_expr_t addr, int have_addr,
		     db_expr_t cnt/* count */, char * mdf/* modif */)
{
  Process *cc =
    have_addr ? (Process *) addr : act_CurContext();
  db_eros_print_keyring(&cc->keyRing);
}

void
db_ctxt_keys_print_cmd(db_expr_t addr, int have_addr,
			db_expr_t cnt/* count */, char * mdf/* modif */)
{
  Process *cc =
    have_addr ? (Process *) addr : act_CurContext();
  db_eros_print_context_keyregs(cc);
}

void
db_activity_print_cmd(db_expr_t addr, int have_addr,
		    db_expr_t cnt/* count */, char * mdf/* modif */)
{
  Activity *t;
  const char * cur_str;

  if (have_addr) {
    t = (Activity *) addr;
    cur_str="";
  } else {
    t = act_Current();
    cur_str="current ";
  }
  
  if (t) {

//    db_eros_print_activity(t);

    db_printf("%sactivity %#x (%s) prio=%#x\n"
              " haz=%d lastq=%#x",
	      cur_str,
	      t, act_stateNames[t->state],
	      /*t->priority*/t->readyQ->mask, t->actHazard, t->lastq);
    if (act_HasProcess(t)) {
      db_printf(" proc=%#x\n ", act_GetProcess(t));
    } else {
      db_printf(" oid=%#llx\n ", t->id.oid);
    }

  } 
  else
    db_printf("No current activity.\n");
}

void
db_inv_print_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		 db_expr_t cnt/* count */, char * mdf/* modif */)
{
  uint32_t invType = inv.invType;
  static char *invTypeName[] = { "npreturn", "preturn",
    "call", "pcall",
    "send", "psend", "keeper" };
  char* theTypeName = "???";

  db_printf("Inv 0x%08x: ", &inv);
  if (inv_IsActive(&inv) == false) {
    db_printf("Not active\n");
    return;
  }
  
  if (invType <= 6)
    theTypeName = invTypeName[invType];
  
  if (inv.key) {
    db_printf("Invoked key 0x%08x ity=%d(%s) kt=%d kd=%d inv count=%llu\n"
	      " OC=0x%08x (%d)\n",
	      inv.key, invType, theTypeName,
	      keyBits_GetType(inv.key), inv.key->keyData,
	      KernStats.nInvoke,
	      inv.entry.code,
	      inv.entry.code);
    db_printf("(0x%08x): ", inv.key);
    db_eros_print_key(inv.key);
  }
  else
    db_printf("Invoked key not yet determined\n");

  if (MapsWereInvalidated()) {
    db_printf("NOTE: invocation will retry\n");
  }
}

void
db_entry_print_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  int i = 0;

  if (inv_IsActive(&inv) == false)
    db_printf("WARNING: no active invocation\n");
  
  for (i = 0; i < 4; i++) {
    if (inv.entry.key[i]) {
      db_printf("%d: (0x%08x): ", i, inv.entry.key[i]);
      db_eros_print_key(inv.entry.key[i]);
    }
  }

  db_printf("w0: 0x%08x w1: 0x%08x w2: 0x%08x w3: 0x%08x\n",
	    inv.entry.code, inv.entry.w1, inv.entry.w2, inv.entry.w3);

  db_printf("String: data 0x%08x len %d inv count=%u\n",
	    inv.entry.data, inv.entry.len,
	    (uint32_t) KernStats.nInvoke);
  db_printf(" str: ");
  db_eros_print_string(inv.entry.data, inv.entry.len);
}

void
db_exit_print_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t cnt/* count */, char * mdf/* modif */)
{
  int i = 0;

  if (inv_IsActive(&inv) == false)
    db_printf("WARNING: No active invocation\n");
  
  for (i = 0; i < 4; i++) {
    if (inv.exit.pKey[i]) {
      db_printf("%d: (0x%08x): ", i, inv.exit.pKey[i]);
      db_eros_print_key(inv.exit.pKey[i]);
    }
  }

  db_printf("w0: 0x%08x w1: 0x%08x w2: 0x%08x w3: 0x%08x\n",
	    inv.exit.code, inv.exit.w1, inv.exit.w2, inv.exit.w3);
  db_printf("String: data 0x%08x len %d inv count=%u\n",
	    inv.exit.data, inv.exit.rcvLen,
	    (uint32_t) KernStats.nInvoke);
}

void
db_invokee_print_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		     db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (inv_IsActive(&inv) == false) {
    if (inv.invokee)
      db_printf("WARNING! Out-of-date invokee!\n");
    else {
      db_printf("No active invocation\n");
      return;
    }
  }

  if (! inv.invokee)
    db_printf("no invokee\n");
  else
    db_eros_print_context(inv.invokee);
}

static Process *
getInvokee(void)
{
  if (! inv_IsActive(&inv)) {
    db_printf("No active invocation\n");
    return NULL;
  }

  if ( inv.key && keyBits_IsGateKey(inv.key) ) {
    return inv.key->u.gk.pContext;
  }
  else if (inv.entry.key[3] && keyBits_IsGateKey(inv.entry.key[3]) ) {
    return inv.entry.key[3]->u.gk.pContext;
  }
  else
    return NULL;
}

void
db_invokee_keys_print_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
			db_expr_t cnt/* count */, char * mdf/* modif */)
{
  Process * cc = getInvokee();
  if (cc)
    db_eros_print_context_keyregs(cc);
}

void
db_show_uactivity_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		    db_expr_t cnt/* count */, char * mdf/* modif */)
{
  uint32_t i = 0;

  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    Activity *t = &act_ActivityTable[i];
    if (t->state != act_Free)
      db_eros_print_activity(t);
  }
}


void
db_reboot_cmd(db_expr_t dt, int it , db_expr_t det, char* ch)
{
  mach_HardReset();
}

/* This is quite tricky.  Basically, we are trying to find the most
 * expensive 10 procedures in the kernel.  The problem is that we
 * don't really want to sort the symbol table to do it.
 */

#ifdef OPTION_KERN_PROFILE
extern uint32_t *KernelProfileTable;
#endif


#ifdef OPTION_KERN_STATS
#define DB64(x) ((uint32_t)(x>>32)), (uint32_t)(x)

void
db_kstat_show_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  db_printf("nDepend   0x%08x%08x  "
	    "nDepMerge 0x%08x%08x\n"

	    "nDepInval 0x%08x%08x  "
	    "nDepMakRO %#16llx  "
	    "nDepTrkRf %#16llx  "
	    "nDepTrkDr %#16llx  "
	    "nDepZap   0x%08x%08x\n"

	    "nInvoke   0x%08x%08x  "
	    "nInvKpr   0x%08x%08x\n"

	    "nPfTraps  0x%08x%08x  "
	    "nPfAccess 0x%08x%08x\n"

	    "nWalkSeg  0x%08x%08x  "
	    "nWalkLoop 0x%08x%08x\n"

	    "nKeyPrep  0x%08x%08d  "
	    "nInter    0x%08x%08d\n"

	    "nGateJmp  0x%08x%08x  "
	    "nInvRetry 0x%08x%08x\n",

	    DB64(KernStats.nDepend),
	    DB64(KernStats.nDepMerge),

	    DB64(KernStats.nDepInval),
            KernStats.nDepMakeRO,
            KernStats.nDepTrackRef,
            KernStats.nDepTrackDirty,
	    DB64(KernStats.nDepZap),

	    DB64(KernStats.nInvoke),
	    DB64(KernStats.nInvKpr),

	    DB64(KernStats.nPfTraps),
	    DB64(KernStats.nPfAccess),

	    DB64(KernStats.nWalkSeg),
	    DB64(KernStats.nWalkLoop),

	    DB64(KernStats.nKeyPrep),
	    DB64(KernStats.nInter),

	    DB64(KernStats.nGateJmp),
	    DB64(KernStats.nInvRetry)
	    );
  KernStats_PrintMD();
}

#ifdef FAST_IPC_STATS
extern "C" {
  extern uint32_t nFastIpcPath;
  extern uint32_t nFastIpcFast;
  extern uint32_t nFastIpcRedSeg;
  extern uint32_t nFastIpcString;
  extern uint32_t nFastIpcSmallString;
  extern uint32_t nFastIpcLargeString;
  extern uint32_t nFastIpcNoString;
  extern uint32_t nFastIpcRcvPf;
  extern uint32_t nFastIpcEnd;
  extern uint32_t nFastIpcOK;
  extern uint32_t nFastIpcPrepared;
}

extern "C" void db_kstat_fast_cmd(db_expr_t, int, db_expr_t, char*);
void
db_kstat_fast_cmd(db_expr_t, int, db_expr_t, char*)
{
  db_printf("nFastIpcPath        0x%08x  "
	    "nFastIpcFast        0x%08x\n"

	    "nFastIpcRedSeg      0x%08x  "
	    "nFastIpcString      0x%08x\n"

	    "nFastIpcSmallString 0x%08x  "
	    "nFastIpcLargeString 0x%08x\n"

	    "nFastIpcNoString    0x%08x  "
	    "nFastIpcRcvPf       0x%08x\n"

	    "nFastIpcEnd         0x%08x  "
	    "nFastIpcOK          0x%08x\n"

	    "nFastIpcPrepared    0x%08x\n"
	    ,

	    nFastIpcPath,
	    nFastIpcFast,
	    nFastIpcRedSeg,
	    nFastIpcString,
	    nFastIpcSmallString,
	    nFastIpcLargeString,
	    nFastIpcNoString,
	    nFastIpcRcvPf,
	    nFastIpcEnd,
	    nFastIpcOK,
	    nFastIpcPrepared
	    );
}
#endif /* FAST_IPC_STATS */

void
db_kstat_clear_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  memset(&KernStats, 0, sizeof(KernStats));
#ifdef OPTION_KERN_TIMING_STATS
  Invocation::ZeroStats();
#endif
}

#ifdef OPTION_KERN_TIMING_STATS
void
db_kstat_ipc_cmd(db_expr_t, int, db_expr_t, char*)
{
  for (int i = 0; i < KKT_NUM_KEYTYPE; i++) {
    if (Invocation::KeyHandlerCounts[i][IT_Call] ||
	Invocation::KeyHandlerCounts[i][IT_Reply] ||
	Invocation::KeyHandlerCounts[i][IT_Send]) {
      db_printf("kt%02d: C [%8U] %13U cy R [%8U] %13U cy\n",
		i,
		Invocation::KeyHandlerCounts[i][IT_Call],
		Invocation::KeyHandlerCycles[i][IT_Call],
		Invocation::KeyHandlerCounts[i][IT_Reply],
		Invocation::KeyHandlerCycles[i][IT_Reply]);
      db_printf("      S [%8U] %13U cy\n",
		Invocation::KeyHandlerCounts[i][IT_Send],
		Invocation::KeyHandlerCycles[i][IT_Send]);
    }
  }
}
#endif /* OPTION_KERN_TIMING_STATS */
#endif /* OPTION_KERN_STATS */

#ifdef OPTION_KERN_PROFILE
typedef int (*qsortfn)(...);
  
int
ddb_CompareFunsByAddr(FuncSym *sn0, FuncSym*sn1)
{
  if (sn0->address < sn1->address)
    return -1;
  if (sn0->address == sn1->address)
    return 0;
  return 1;
}

int
ddb_CompareLinesByAddr(LineSym *sn0, LineSym*sn1)
{
  if (sn0->address < sn1->address)
    return -1;
  if (sn0->address == sn1->address)
    return 0;
  return 1;
}

/* Note - higher sorts first: */
int
ddb_CompareFunsByCount(FuncSym *sn0, FuncSym*sn1)
{
  if (sn0->profCount > sn1->profCount)
    return -1;
  if (sn0->address == sn1->address)
    return 0;
  return 1;
}

enum HowSorted {
  Unknown,
  ByAddr,
  ByCount
};

static HowSorted FunSort = Unknown;
static HowSorted LineSort = Unknown;

void
ddb_SortLinesByAddr()
{
  if (LineSort != ByAddr) {
    qsort(LineSym::table, LineSym::count, sizeof(LineSym),
	  (qsortfn)ddb_CompareLinesByAddr);
    LineSort = ByAddr;
  }
}

void
ddb_SortFunsByAddr()
{
  if (FunSort != ByAddr) {
    qsort(FuncSym::table, FuncSym::count, sizeof(FuncSym),
	  (qsortfn)ddb_CompareFunsByAddr);
    FunSort = ByAddr;
  }
}

void
ddb_SortFunsByCount()
{
  if (FunSort != ByCount) {
    qsort(FuncSym::table, FuncSym::count, sizeof(FuncSym),
	  (qsortfn)ddb_CompareFunsByCount);
    FunSort = ByCount;
  }
}

void
db_prof_clear_cmd(db_expr_t, int, db_expr_t, char*)
{
  uint32_t kernelCodeLength = (uint32_t) &db_etext;
  uint32_t tableSizeInWords = (kernelCodeLength >> 4);
  
  for (uint32_t i = 0; i < tableSizeInWords; i++)
    KernelProfileTable[i] = 0;
}

static uint64_t
prep_prof()
{
  uint64_t totCount = 0ll;
  
  /* One profile word for every 16 bytes: */
  
  const int topsz = 10;
  
  FuncSym *top[topsz];

  for (int i = 0; i < topsz; i++)
    top[i] = 0;

  uint32_t kernelCodeLength = (uint32_t) &db_etext;
  uint32_t tableSizeInWords = (kernelCodeLength >> 4);
  
  for (uint32_t i = 0; i < tableSizeInWords; i++)
    totCount += KernelProfileTable[i];

  ddb_SortFunsByAddr();

  /* Symbol table is sorted by address.  We take advantage of that
   * here for efficiency:
   */
  uint32_t limit = (uint32_t) &db_etext;

  for (uint32_t i = 0; i < FuncSym::count; i++) {
    if (FuncSym::table[i].address >= limit)
      continue;
    
    FuncSym::table[i].profCount = 0;
    uint32_t address = FuncSym::table[i].address;
    address += 0xf;
    address &= ~0xfu;

    while (address < FuncSym::table[i+1].address) {
      FuncSym::table[i].profCount += KernelProfileTable[address >> 4];
      address += 16;
    }
  }

  ddb_SortFunsByCount();

  return totCount;
}

static void
show_prof(uint64_t tot, uint32_t limit)
{
  uint32_t printCount = 0;

  for (uint32_t i = 0; i < FuncSym::count; i++) {
    if (FuncSym::table[i].profCount == 0)
      continue;
    printCount++;
    db_printf("%02d: %-10u 0x%08x  %s\n",
	      i, FuncSym::table[i].profCount,
	      FuncSym::table[i].address,
	      FuncSym::table[i].name);
    if (printCount >= limit)
      break;
  }

  db_printf("Done.  Total ticks: %#llx\n", tot);
}

void
db_prof_top_cmd(db_expr_t, int, db_expr_t, char*)
{
  uint64_t totCount = prep_prof();

  show_prof(totCount, 20);
}

void
db_prof_all_cmd(db_expr_t, int, db_expr_t, char*)
{
  uint64_t totCount = prep_prof();

  show_prof(totCount, UINT32_MAX);
}
#endif

void
db_show_ioreqs_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  extern void db_show_ioreqs(void);
  db_show_ioreqs();
}

void
db_show_irq_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  db_printf("Show IRQ temporarily disabled... surgery in progress\n");
#if 0
  db_printf("IRQ disable depth is %d\n", IRQ::DISABLE_DEPTH());
	    
  for (uint32_t i = 0; i < NUM_HW_INTERRUPT; i++) {
    IntAction *ia = IRQ::ddb_getaction(i);
    if (ia && ia->IsValid()) {
      db_printf("IRQ %d: \"%s\" (%s)\n", ia->GetIrq(),
		ia->DriverName(),
		ia->IsWired() ? "wired" : "not wired");
    }
  }
#endif
}


void
db_kstat_hist_depend_cmd(db_expr_t dt, int  it, db_expr_t det, char* ch)
{
  int t;
  uint32_t bucket;
  
  t = db_read_token();
  
  if (t != tNUMBER)
    Depend_ddb_dump_hist();
  
  bucket = db_tok_number;
	
  t = db_read_token();
  if (db_read_token() != tEOL) {
    db_error("?\n");
    /*NOTREACHED*/
  }

  Depend_ddb_dump_bucket(bucket);
}


void
db_kstat_hist_objhash_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  int t;
  uint32_t bucket;
  
  t = db_read_token();
  
  if (t != tNUMBER)
    objH_ddb_dump_hash_hist();
  
  bucket = db_tok_number;
	
  t = db_read_token();
  if (db_read_token() != tEOL) {
    db_error("?\n");
    /*NOTREACHED*/
  }

  objH_ddb_dump_bucket(bucket);
}


void
db_show_sources(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  objC_ddb_DumpSources();
}


void
db_check_nodes_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  if (check_Nodes())
    db_printf("nodes are okay\n");
  else
    db_printf("nodes are crocked\n");
  
}

void
db_check_pages_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  if (check_Pages())
    db_printf("pages are okay\n");
  else
    db_printf("pages are crocked\n");
  
}

void
db_check_procs_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  if (check_Contexts("ddb"))
    db_printf("processes are okay\n");
  else
    db_printf("processes are crocked\n");
}

void
db_show_key_cmd(db_expr_t addr, int have_addr, db_expr_t det, char* ch)
{
  if (have_addr == 0)
    db_error("requires address\n");

  db_eros_print_key(((Key *) addr));
  db_eros_print_key_details(((Key *) addr));
}

void
db_show_node_cmd(db_expr_t addr, int have_addr, db_expr_t det, char* ch)
{
  if (have_addr == 0)
    db_error("requires address\n");

  db_eros_print_node(((Node *) addr));
}

void
db_show_obhdr_cmd(db_expr_t addr, int have_addr, db_expr_t det, char* ch)
{
  if (have_addr == 0)
    db_error("requires address\n");

  objH_ddb_dump((ObjectHeader *) addr);
}

void
db_show_readylist_cmd(db_expr_t addr, int have_addr, db_expr_t det, char* ch)
{
  extern void db_show_readylist(void);
  db_show_readylist();
}

void
db_show_pins_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  objC_ddb_dump_pinned_objects();
}


void
db_show_pmem_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  physMem_ddb_dump();
}


void
db_show_pte_cmd(db_expr_t addr, int have_addr, db_expr_t det, char* ch)
{
  if (have_addr == 0)
    db_error("requires address\n");

  pte_ddb_dump((PTE *) addr);
}

void
db_show_pages_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  objC_ddb_dump_pages();
}

void
db_show_nodes_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  objC_ddb_dump_nodes();
}

void
db_show_procs_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  objC_ddb_dump_procs();
}

void
db_user_continue_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  continue_user_bpt = true;
}

void
db_node_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  int	t;
  OID     oid;
  Node *pNode = 0;

  t = db_read_token();
  if (t != tNUMBER)
    db_error("expects OID\n");

  oid = db_tok_number;
	
  t = db_read_token();
  if (db_read_token() != tEOL) {
    db_error("?\n");
    /*NOTREACHED*/
  }

  pNode = objH_LookupNode(oid);
  if (pNode == 0)
    db_error("not in core\n");

  db_eros_print_node(pNode);
}

void
db_show_counters_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  int	t;
  OID     oid;
  Node *pNode = 0;
  bool isProcess;

  t = db_read_token();
  if (t != tNUMBER)
    db_error("expects OID\n");

  oid = db_tok_number;
	
  t = db_read_token();
  if (db_read_token() != tEOL) {
    db_error("?\n");
    /*NOTREACHED*/
  }

  pNode = objH_LookupNode(oid);
  if (pNode == 0)
    db_error("not in core\n");

  ObjectHeader * pObj = node_ToObj(pNode);
  db_printf("Node oid=%#llx ac=0x%08x cc=0x%08x\n",
	    pObj->oid,
	    objH_GetAllocCount(pObj),
	    node_GetCallCount(pNode));

#ifndef NDEBUG
  if (node_Validate(pNode) == false)
    db_error("...Is not valid\n");
#endif
  
  isProcess =
    BOOL((pNode->node_ObjHdr.obType == ot_NtProcessRoot ||
     pNode->node_ObjHdr.obType == ot_NtRegAnnex) &&
    pNode->node_ObjHdr.prep_u.context);

  if (isProcess) {
    db_printf("     count = %#llx\n", pNode->node_ObjHdr.prep_u.context->stats.evtCounter0);
  }
  else {
    Key* k /*@ not null @*/ = &pNode->slot[10];
    uint32_t hi = k->u.nk.value[1];
    uint32_t lo = k->u.nk.value[2];
    db_printf("     count = 0x%08x%08x\n",
	      hi, lo);
  }
}

void
db_page_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  int	t;
  OID     oid;
  kva_t kva;

  t = db_read_token();
  if (t != tNUMBER)
    db_error("expects OID\n");

  oid = db_tok_number;
	
  t = db_read_token();
  if (db_read_token() != tEOL) {
    db_error("?\n");
    /*NOTREACHED*/
  }
  
  ObjectHeader * pObj = objH_Lookup(oid, 0);
  if (! pObj || objH_GetBaseType(pObj) != capros_Range_otPage)
    db_error("not in core\n");

  PageHeader * pageH = objH_ToPage(pObj);

  kva = pageH_GetPageVAddr(pageH);
  
  db_printf("Page (hdr=0x%08x, data=0x%08x) oid=%#llx ac=0x%08x ot=%d\n",
	    pageH,
	    kva,
	    pageH_ToObj(pageH)->oid,
	    objH_GetAllocCount(pageH_ToObj(pageH)),
	    pageH_GetObType(pageH));
}

void
db_pframe_cmd(db_expr_t dt, int it, db_expr_t det, char* ch)
{
  int	t;
  uint32_t  frame;
  kva_t kva;

  t = db_read_token();
  if (t != tNUMBER)
    db_error("expects physAddr\n");

  frame = db_tok_number;
	
  t = db_read_token();
  if (db_read_token() != tEOL) {
    db_error("?\n");
    /*NOTREACHED*/
  }

  PageHeader * pObj = objC_PhysPageToObHdr(frame);
  if (pObj == 0)
    db_error("not in core\n");

  kva = pageH_GetPageVAddr(pObj);

  db_printf("Frame (hdr=0x%08x, data=0x%08x) ot=%d\n",
	    pObj,
	    kva,
	    pageH_GetObType(pObj));
  if (pageH_IsObjectType(pObj))
    db_printf("    oid=%#llx ac=0x%08x\n",
              pageH_ToObj(pObj)->oid,
              objH_GetAllocCount(pageH_ToObj(pObj)));
}

void
pte_print(uint32_t addr, char *note, PTE *pte)
{
  db_printf("0x%08x %s ", addr, note);
  pte_ddb_dump(pte);
}

void
db_show_mappings_cmd(db_expr_t addr, int have_addr, db_expr_t det, char* ch)
{
  int	t;
  
  t = db_read_token();
  if (t != tNUMBER)
    db_error("expects space base nPage\n");

  uint32_t space = db_tok_number;
	
  t = db_read_token();
  if (t != tNUMBER)
    db_error("expects space base nPage\n");

  uint32_t base = db_tok_number;
  
  t = db_read_token();
  if (t != tNUMBER)
    db_error("expects space base nPage\n");

  uint32_t nPages = db_tok_number;
  
  t = db_read_token();
  if (db_read_token() != tEOL) {
    db_error("?\n");
    /*NOTREACHED*/
  }

  if ((uint32_t)space & EROS_PAGE_MASK)
    db_error("base must be a page address\n");

  db_show_mappings_md(space, base, nPages);
}

void
db_print_segwalk(const SegWalk *wi)
{
#define BOOLC(x) ((x) ? 'y' : 'n')
  db_printf("wi: memObj 0x%08x restrictions 0x%x\n"
	    "offset: 0x%x%08x\n"
            "keeperGPT 0x%x backgroundGPT 0x%x\n"
	    "segFault %d needWrite %c traverseCount %d\n",
	    wi->memObj, wi->restrictions,
	    (uint32_t) (wi->offset >> 32),
	    (uint32_t) (wi->offset),
            wi->keeperGPT, wi->backgroundGPT,
	    wi->faultCode, BOOLC(wi->needWrite), wi->traverseCount);
#undef BOOLC
}      

void
db_show_walkinfo_cmd(db_expr_t addr, int have_addr,
		     db_expr_t cnt/* count */, char * mdf/* modif */)
{
  if (have_addr == 0)
    db_error("requires address\n");

  db_print_segwalk((const SegWalk *) addr);
}

void
db_addrspace_cmd(db_expr_t addr, int have_addr,
		 db_expr_t cnt/* count */, char * mdf/* modif */)
{
  Process * proc =
    have_addr ? (Process *) addr : act_CurContext();
  mach_LoadAddrSpace(proc);
}
