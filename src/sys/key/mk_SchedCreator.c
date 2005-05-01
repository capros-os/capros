/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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
#include <kerninc/Key.h>
#include <kerninc/CpuReserve.h>
#include <eros/Invoke.h>
#include <eros/Reserve.h>
#include <eros/SchedCreKey.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/Machine.h>
#include <eros/StdKeyType.h>

#include <idl/eros/key.h>

void
SchedCreatorKey(Invocation* inv /*@ not null @*/)
{
#ifndef OPTION_PURE_EXIT_STRINGS
  if (inv->entry.code == OC_SchedCre_Get)
    proc_SetupExitString(inv->invokee, inv, sizeof(struct CpuReserveInfo));
#endif
#ifndef OPTION_PURE_ENTRY_STRINGS

  if (inv->entry.code == OC_SchedCre_Set)
    proc_SetupEntryString(act_CurContext(), inv);

#endif

  COMMIT_POINT();
      
  switch(inv->entry.code) {
  case OC_SchedCre_GetLimit:
    {
      inv->exit.w1 = MAX_CPU_RESERVE;
      break;
    }
    /*#if 0*/
  case OC_SchedCre_Get:
    {
#if CONV
      uint32_t ndx;
      struct CpuReserveInfo rsrvinfo;
      /*Reserve *rsrv = 0;*/
      
      ndx = inv->entry.w1;
      if (ndx < STD_CPU_RESERVE || ndx >= MAX_CPU_RESERVE) {
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }
      
      inv_CopyOut(inv, sizeof(rsrvinfo), &rsrvinfo);
#endif
      printf("SchedCreator in kernel\n");
      break;
    }
  case OC_SchedCre_Set:
    {
      /*Reserve *rsrv = 0;*/
      uint32_t ndx;
      uint32_t period;
      uint32_t duration;
#if 0
      if (inv->entry.len < sizeof(struct CpuReserveInfo)) {
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }

    
      inv_CopyIn(inv, sizeof(struct CpuReserveInfo), &ri);
#endif

      ndx = inv->entry.w1;
      duration = inv->entry.w2;
      period = inv->entry.w3;

      if (ndx < 0 || ndx >= 32) {
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }
      
      printf("in kernel, d = %d", duration);
      printf(" index %d", ndx);
      printf(" p = %d\n", period);
#if 0
      rsrv /*@ not null @*/ = &cpuR_CpuReserveTable[ri.index];

      tim_Disarm(&rsrv->reserveTimer);

      
      rsrv->period = mach_MillisecondsToTicks(ri.period);
      rsrv->duration = mach_MillisecondsToTicks(ri.duration);
      rsrv->quanta = mach_MillisecondsToTicks(ri.quanta);
      rsrv->start = mach_MillisecondsToTicks(ri.start);
      rsrv->rsrvPrio = ri.rsrvPrio;
      rsrv->normPrio = ri.normPrio;

      
      rsrv->residQuanta = rsrv->quanta;
      rsrv->residDuration = rsrv->duration;
      if (rsrv->residDuration)
	rsrv->residDuration -= rsrv->quanta;


      /*cpuR_Reset(rsrv);*/
#endif

      /* set reserve info in reserve table */
      res_SetReserveInfo(period, duration, ndx);
      if (inv->exit.pKey[0]) {
        KeyBits newSchedKey;
        keyBits_InitToVoid(&newSchedKey);

        
        keyBits_InitType(&newSchedKey, KKT_Sched);
	newSchedKey.keyData = ndx;

        /* set the high bit so key can be identified
           as reserve key
        */
        newSchedKey.keyData |= (1u<<pr_Reserve);
        printf("in SchedCreator - returning key data = %d\n", newSchedKey.keyData);
        inv_SetExitKey(inv, 0, &newSchedKey);
      }
      /* Rest of void key is right for this call. */
      break;
    }
    /*#endif*/

  case OC_SchedCre_MkPrio:
    {
      if (inv->exit.pKey[0]) {
	Key k;			/* temporary in case send and receive */
				/* slots are the same. */
        
        keyBits_InitToVoid(&k);
	keyBits_InitType(&k,  KKT_Sched);
	k.keyData = inv->entry.w1;

	inv_SetExitKey(inv, 0, &k);

	/* Key /k/ not prepared, so no need to unwind it's link
	   chain. */
      }

      inv->exit.code = RC_OK;
      break;
    }


  case OC_eros_key_getType:
    {
      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_SchedCreator;
      break;
    }
  default:
    inv->exit.code = RC_eros_key_UnknownRequest;
    break;
  }
  return;
}
