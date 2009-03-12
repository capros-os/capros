#ifndef __PROCESS_INLINE_H__
#define __PROCESS_INLINE_H__
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

#include <kerninc/Process.h>
#include <kerninc/Activity.h>
#include <kerninc/Node.h>

INLINE Process *
proc_Current(void)
{
  assert((proc_curProcess == NULL && (act_Current() == NULL
                                      || ! act_HasProcess(act_Current())))
         || (act_GetProcess(act_Current()) == proc_curProcess));
  return proc_curProcess;	// could be NULL
}

// This procedure's name is historical.
INLINE Process *
act_CurContext()
{
  Process * p = proc_Current();
  assert(p);
  return p;
}

INLINE bool 
proc_IsWellFormed(Process* thisPtr)
{
#ifndef NDEBUG
  if (thisPtr->faultCode == capros_Process_FC_MalformedProcess) {
    assert (thisPtr->processFlags & capros_Process_PF_FaultToProcessKeeper);
  }
#endif
  if (thisPtr->hazards & (hz_DomRoot | hz_KeyRegs | hz_Malformed
#ifdef EROS_HAVE_FPU
                          | hz_FloatRegs
#endif
                         )) {
    return false;
  }
  else return true;
}

#ifndef NDEBUG
/* Returns true iff p points within the Process area. */
INLINE bool 
IsInProcess(const void * p)
{
  if ( ((uint32_t) p >= (uint32_t) proc_ContextCache) &&
       ((uint32_t) p <
        (uint32_t) &proc_ContextCache[KTUNE_NCONTEXT] ) ) {
    return true;
  }
  return false;
}
#endif

INLINE void 
proc_ClearFault(Process * thisPtr)
{
  thisPtr->faultCode = capros_Process_FC_NoFault;
  thisPtr->faultInfo = 0;
  thisPtr->processFlags &= ~capros_Process_PF_FaultToProcessKeeper;
}

INLINE void 
proc_SetMalformed(Process* thisPtr)
{
#ifdef OPTION_DDB
  /* This error is most likely of interest to the kernel developer,
     so for now: */
  dprintf(true, "Process is malformed\n");
#endif
  proc_SetFault(thisPtr, capros_Process_FC_MalformedProcess, 0);
  thisPtr->hazards |= hz_Malformed; 
}

INLINE bool 
proc_IsExpectingMsg(Process * thisPtr)
{
  return thisPtr->processFlags & capros_Process_PF_ExpectingMessage;
}

INLINE bool
proc_HasDevicePrivileges(Process * thisPtr)
{
  assert((thisPtr->hazards & hz_DomRoot) == 0);
  assert((thisPtr->hazards & hz_KeyRegs) == 0);

  return keyBits_IsType(&thisPtr->procRoot->slot[ProcIoSpace], KKT_DevicePrivs);
}

/* Return whether this process is part of the user-mode Page Fault Handler
   (mainly, the disk driver). */
INLINE bool
proc_IsPFH(const Process * proc /* may be NULL */ )
{
  if (! proc || proc_IsKernel(proc))
    return false;

  /* We used to have:
   return ! OIDIsPersistent(node_ToObj(proc->procRoot)->oid);
   The following is faster:
   return ! objH_GetFlags(node_ToObj(proc->procRoot), OFLG_Cleanable);
  since almost every non-persistent process
  is likely to be working on behalf of the PFH.
  But that doesn't work, because in its initialization drivers may
  register with nplink (i.e. access persistent data).
  So we have instead: */
  return proc->kernelFlags & KF_PFH;
  /* This is actually flawed because many non-persistent processes fail to
  register as part of the PFH even though they are working on behalf of
  the PFH (e.g. VCSK, SuperNode). */
}

#endif /* __PROCESS_INLINE_H__ */
