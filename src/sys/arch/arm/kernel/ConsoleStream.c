/*
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <kerninc/KernStream.h>

void
ConsoleStream_Init(void)
{
  /* No console on EDB9315 - use OPTION_OUTPUT_ON_TTY0 */
}

void
ConsoleStream_Put(uint8_t c)
{
  /* No console on EDB9315 - use OPTION_OUTPUT_ON_TTY0 */
}

struct KernStream TheConsoleStream = {
  ConsoleStream_Init,
    ConsoleStream_Put
#ifdef OPTION_DDB
    ,
    ConsoleStream_Get,
    ConsoleStream_SetDebugging,
    ConsoleStream_EnableDebuggerInput
#endif
    };

KernStream* kstream_ConsoleStream = &TheConsoleStream;
