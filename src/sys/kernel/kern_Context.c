/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
#include <kerninc/Process.h>
#include <kerninc/Key.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Node.h>
#include <kerninc/ReadyQueue.h>

/* replacement for Process::Process() */
void
proc_Init(Process *thisPtr)
{
  int i = 0;
  int k = 0;
  keyR_ResetRing(&thisPtr->keyRing);
  sq_Init(&thisPtr->stallQ);
  for (k = 0; k < EROS_PROCESS_KEYREGS; k++)
    keyBits_InitToVoid(&thisPtr->keyReg[k]);

  keyBits_InitToVoid(&thisPtr->lastInvokedKey);

  thisPtr->isUserContext = true;		/* until proven otherwise */
  thisPtr->readyQ = dispatchQueues[pr_Never];
  thisPtr->faultCode = FC_NoFault;
  thisPtr->faultInfo = 0;
  thisPtr->processFlags = 0;
  thisPtr->saveArea = 0;
  thisPtr->hazards = 0u;	/* deriver should change this! */
  thisPtr->curActivity = 0;

  for (i = 0; i < 8; i++)
    thisPtr->name[i] = '?';
  thisPtr->name[7] = 0;
}
