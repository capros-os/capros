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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/SysTimer.h>
#include <kerninc/Machine.h>
#include <kerninc/Activity.h>
#include <kerninc/KernStats.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/arch/i386/SysTrace.h>

const char *mach_ModeName(uint32_t mode);
bool mach_SetCounterMode(uint32_t mode);
void mach_ClearCounters();
void mach_EnableCounters();
void mach_DisableCounters();
uint64_t mach_ReadCounter(uint32_t which);

extern void zapcounters();
extern uint64_t rdtsc();
uint32_t setup_value;

#ifdef OPTION_KERN_STATS
struct KernStats_s  KernStats;
#endif

#ifdef OPTION_KERN_TIMING_STATS
extern uint64_t inv_delta_cy;
extern uint32_t inv_delta_reset;
extern uint64_t inv_handler_cy;
extern uint64_t pf_delta_cy;
extern uint64_t kpr_delta_cy;
#ifdef OPTION_KERN_EVENT_TRACING
extern uint64_t inv_delta_cnt0;
extern uint64_t inv_delta_cnt1;
extern uint64_t pf_delta_cnt0;
extern uint64_t pf_delta_cnt1;
extern uint64_t kpr_delta_cnt0;
extern uint64_t kpr_delta_cnt1;
#endif
#endif

#ifdef FAST_IPC_STATS
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
#endif

uint32_t src_ok = 0;
uint32_t dest_ok = 0;
uint32_t copy_ok = 0;
uint32_t move_string = 0;
uint32_t fast_ok = 0;
uint32_t state_ok = 0;
uint32_t totmov = 0;
uint64_t bytes_moved = 0;

/* May Yield. */
void
SysTraceKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

  static uint64_t startcy = 0;
  static uint64_t startTick = 0ll;
  static uint64_t startInvoke = 0ll;
  static uint64_t startInter = 0ll;
  static int32_t activeMode = 0;
  
  if (inv->entry.code == OC_capros_arch_i386_SysTrace_reportCounter)
    proc_SetupExitString(inv->invokee, inv, sizeof(struct capros_arch_i386_SysTrace_info));

  COMMIT_POINT();
  
  switch(inv->entry.code) {
  case OC_capros_key_getType:
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_SysTrace;
    break;

  default:
    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  case OC_capros_arch_i386_SysTrace_startCounter:
    {
      /* Start counted behavior */

      startInvoke = KernStats.nInvoke;
      startInter = KernStats.nInter;
      totmov = move_string = state_ok = fast_ok = 0;
      src_ok = dest_ok = copy_ok = 0; 
      bytes_moved = 0; 

#ifdef FAST_IPC_STATS
      nFastIpcPath = 0;
      nFastIpcFast = 0;
      nFastIpcRedSeg = 0;
      nFastIpcString = 0;
      nFastIpcSmallString = 0;
      nFastIpcLargeString = 0;
      nFastIpcNoString = 0;
      nFastIpcRcvPf = 0;
      nFastIpcEnd = 0;
      nFastIpcOK = 0;
      nFastIpcPrepared = 0;
#endif

#ifdef OPTION_KERN_TIMING_STATS
      pf_delta_cy = 0ll;
      kpr_delta_cy = 0ll;
      inv_delta_cy = 0ll;
      inv_delta_reset = 1;
      inv_handler_cy = 0ll;
#ifdef OPTION_KERN_EVENT_TRACING
      pf_delta_cnt0 = 0ll;
      pf_delta_cnt1 = 0ll;
      kpr_delta_cnt0 = 0ll;
      kpr_delta_cnt1 = 0ll;
      inv_delta_cnt0 = 0ll;
      inv_delta_cnt1 = 0ll;
#endif
#endif

      activeMode = -1;
      
      
      if (mach_SetCounterMode(inv->entry.w1) == false) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }
      
      
      activeMode = inv->entry.w1;


      mach_ClearCounters();


      startTick = sysT_Now();
     
      startcy = rdtsc();
      mach_EnableCounters();

      
      inv->exit.code = RC_OK;
      break;
    }
  case OC_capros_arch_i386_SysTrace_reportCounter:
    {
      struct capros_arch_i386_SysTrace_info st;
      uint64_t endcy;

      if (activeMode == -1) {
	kzero(&st, sizeof(st));
	inv->exit.code = RC_capros_key_NoAccess;
	break;
      }


      mach_DisableCounters();


      endcy = rdtsc();


      st.traceMode = activeMode;
      st.count0 = mach_ReadCounter(0);
      st.count1 = mach_ReadCounter(1);
      st.cycles = (endcy - startcy);


      inv_CopyOut(inv, sizeof(st), &st);

      
      inv->exit.code = RC_OK;
      break;
    }
  case OC_capros_arch_i386_SysTrace_stopCounter:
    {
#if 0 // Performance test of check.
      uint64_t endcy;
      uint64_t cy;
      uint64_t endInvoke;
      const char *modeName = mach_ModeName(activeMode);
      uint64_t cntr0;
      uint64_t cntr1;
      
      if (activeMode == -1) {
	inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      mach_DisableCounters();


      endcy = rdtsc();
      cy = endcy - startcy;
      endInvoke = KernStats.nInvoke - startInvoke;
#if 0
      uint64_t endIntCount = KernStats.nInter - startInter;
#endif
    
#if 0
      printf("startcy 0x%x%08x endcy 0x%x%08x\n",
		     (uint32_t) (startcy >> 32),
		     (uint32_t) (startcy),
		     (uint32_t) (endcy >> 32),
		     (uint32_t) (endcy));
#endif



      cntr0 = mach_ReadCounter(0);
      cntr1 = mach_ReadCounter(1);


      printf("Cycles: %13U %-7s S: %13U U+S: %13U IC: %u\n",
		     cy,
		     modeName, cntr0, cntr1,
		     endInvoke);
#ifdef FAST_IPC_STATS
      printf("FstPth: %u FstFst %u FstNoStr %u FstEnd %u FstOK %u\n",
		     nFastIpcPath, nFastIpcFast, nFastIpcNoString,
		     nFastIpcEnd, nFastIpcOK);
      printf("FstStr: %u FstSmallStr %u FstLrgStr %u FstRcvPf %u\n",
		     nFastIpcString, 
		     nFastIpcSmallString, 
		     nFastIpcLargeString,
		     nFastIpcRcvPf);
      printf("FstRed: %u  FstPrep %u\n",
		     nFastIpcRedSeg, 
		     nFastIpcPrepared); 
#endif
#ifdef OPTION_KERN_TIMING_STATS
      if (inv_delta_cy)
	printf("KeyInv: %13U KeyFn: %13U Kpr: %13U\n",
		       inv_delta_cy, inv_handler_cy, kpr_delta_cy);
      if (pf_delta_cy)
	printf("Pgflt:  %13U\n",
		       pf_delta_cy, kpr_delta_cy);
#ifdef OPTION_KERN_EVENT_TRACING
      if (pf_delta_cnt0)
	printf(Evt "KeyInv: %-7s S: %13U U+S: %13U\n",
		       modeName,  inv_delta_cnt0, inv_delta_cnt1);
      if (pf_delta_cnt0)
	printf("Evt Pflt:   %-7s S: %13U U+S: %13U\n",
		       modeName,  pf_delta_cnt0, pf_delta_cnt1);
      if (kpr_delta_cnt0)
	printf("Evt Keeper: %-7s S: %13U U+S: %13U\n",
v		       modeName,  kpr_delta_cnt0, kpr_delta_cnt1);
#endif
#endif

#ifdef OPTION_KERN_TIMING_STATS
      {
	int count = 0;
	for (int i = 0; i < KKT_NUM_KEYTYPE; i++) {
	  uint64_t keycount =
	    Invocation::KeyHandlerCounts[i][IT_Call] +
	    Invocation::KeyHandlerCounts[i][IT_Reply] + 
	    Invocation::KeyHandlerCounts[i][IT_Send];
	  uint64_t keycy =
	    Invocation::KeyHandlerCycles[i][IT_Call] +
	    Invocation::KeyHandlerCycles[i][IT_Reply] + 
	    Invocation::KeyHandlerCycles[i][IT_Send];
	  if (keycount) {
	    printf("  kt%02d: [%8U] %13U",
			   i,
			   keycount,
			   keycy);
	    count++;
	  }
	  if (count == 2) {
	    printf("\n");
	    count = 0;
	  }
	}

	if (count != 0)
	  printf("\n");
      }
#endif

#else
// Performance test of check.
#include <kerninc/Check.h>
int i;
for (i=0; i<10; i++) {
  check_Consistency("systrace");
}
#endif

      inv->exit.code = RC_OK;
      break;
    }
  case OC_capros_arch_i386_SysTrace_stopCounterVerbose:
    {
      uint64_t endcy;
      uint64_t cy;
      uint64_t endInter;
      uint64_t endInvoke;
      uint64_t cntr0;
      uint64_t cntr1;
      const char *modeName = mach_ModeName(activeMode);
      uint64_t endTick;
      uint64_t dwticks;
      uint64_t ticks;
      uint64_t dms;
      uint32_t ms;
      
      if (activeMode == -1) {
	inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      mach_DisableCounters();


      endcy = rdtsc();
      cy = endcy - startcy;
      endInter = KernStats.nInter - startInter;
      endInvoke = KernStats.nInvoke - startInvoke;

      cntr0 = mach_ReadCounter(0);
      cntr1 = mach_ReadCounter(1);
      
      endTick = sysT_Now();

      dwticks = endTick - startTick;
      ticks = (uint32_t) dwticks;
      dms = mach_TicksToMilliseconds(dwticks);
      ms = (uint32_t) dms;

    
      printf("Cycles: %13U %-7s S: %13U U+S: %13U IC: %u\n",
		     cy,
		     modeName,
		     cntr0,
		     cntr1,
		     endInter);

#ifdef OPTION_KERN_TIMING_STATS
      if (inv_delta_cy)
	printf("KeyInv: %13U KeyFn: %13U Kpr: %13U\n",
		       inv_delta_cy, inv_handler_cy, kpr_delta_cy);
      if (pf_delta_cy)
	printf("Pgflt:  %13U\n",
		       pf_delta_cy, kpr_delta_cy);
#ifdef OPTION_KERN_EVENT_TRACING
      if (pf_delta_cnt0)
	printf(Evt "KeyInv: %-7s S: %13U U+S: %13U\n",
		       modeName,  inv_delta_cnt0, inv_delta_cnt1);
      if (pf_delta_cnt0)
	printf("Evt Pflt:   %-7s S: %13U U+S: %13U\n",
		       modeName,  pf_delta_cnt0, pf_delta_cnt1);
      if (kpr_delta_cnt0)
	printf("Evt Keeper: %-7s S: %13U U+S: %13U\n",
		       modeName,  kpr_delta_cnt0, kpr_delta_cnt1);
#endif
#endif

      printf("  %u ticks %u ms %u stdInvoke: %U\n",
		     (uint32_t) ticks, ms, endInvoke);

      printf("  state %u fast %u str %u src %u dest %u copy %u move %u\n",
		     state_ok, fast_ok, move_string, src_ok, dest_ok,
		     copy_ok, totmov);
      printf("  bytes moved 0x%08x%08x\n",
		     (uint32_t) (bytes_moved>>32), (uint32_t) bytes_moved);

#ifdef OPTION_KERN_TIMING_STATS
      {
	int count = 0;
	for (int i = 0; i < PRIMARY_KEY_TYPES; i++) {
	  uint64_t keycount =
	    Invocation::KeyHandlerCounts[i][IT_Call] +
	    Invocation::KeyHandlerCounts[i][IT_Reply] + 
	    Invocation::KeyHandlerCounts[i][IT_Send];
	  uint64_t keycy =
	    Invocation::KeyHandlerCycles[i][IT_Call] +
	    Invocation::KeyHandlerCycles[i][IT_Reply] + 
	    Invocation::KeyHandlerCycles[i][IT_Send];
	  if (keycount) {
	    printf("  kt%02d: [%8U] %13U",
			   i,
			   keycount,
			   keycy);
	    count++;
	  }
	  if (count == 2) {
	    printf("\n");
	    count = 0;
	  }
	}
      }
#endif

      inv->exit.code = RC_OK;
      break;
    }
  case OC_capros_arch_i386_SysTrace_clearKernelStats:
    {
#ifdef OPTION_KERN_TIMING_STATS
      Invocation::ZeroStats();
#endif
      
      kzero(&KernStats, sizeof(KernStats));
      inv->exit.code = RC_OK;
      break;
    }
  case OC_capros_arch_i386_SysTrace_getCycle:
    {
      uint64_t cy = rdtsc();
      inv->exit.code = RC_OK;
      inv->exit.w1 = (cy >> 32);
      inv->exit.w2 = cy;
      inv->exit.w3 = 0;
      break;
    }
  }
  ReturnMessage(inv);
}
