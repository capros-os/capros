/*
 * Copyright (C) 2006-2008, 2010, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <kerninc/KernStream.h>

#define WRAP_LINES

// Architecture-specific procedures:
void SerialStream_MachInit(void);
void SerialStream_RawOutput(uint8_t c);
#ifdef OPTION_DDB
uint8_t SerialStream_Get(void);
void SerialStream_SetDebugging(bool onOff);
void SerialStream_EnableDebuggerInput(void);
#endif

#ifdef WRAP_LINES

unsigned int curCol;	// column where the cursor is at, 0 == first column
#define MaxColumns 80
#define TABSTOP 8

static void
SpacingChar(void)
{
  if (curCol >= MaxColumns) {
    curCol = 0;
    SerialStream_RawOutput(CR);
    SerialStream_RawOutput(LF);
  }
  curCol++;
}

#endif

void
SerialStream_Init(void)
{
#ifdef WRAP_LINES
  curCol = 0;
#endif

  SerialStream_MachInit();
}

void
SerialStream_Put(uint8_t c)
{
#ifdef WRAP_LINES
  if (kstream_IsPrint(c)) {
    SpacingChar();
  }
  else {
    /* Handle the non-printing characters: */
    switch (c) {
    case BS:
      if (curCol)
	curCol--;
      break;
    case TAB:
      SpacingChar();
      while (curCol % TABSTOP)
        SpacingChar();
      break;
    case CR:
      curCol = 0;
      break;
    default:
      break;
    }
  }
#endif
  SerialStream_RawOutput(c);
}

struct KernStream TheSerialStream = {
  SerialStream_Init,
  SerialStream_Put
#ifdef OPTION_DDB
  ,
  SerialStream_Get,
  SerialStream_SetDebugging,
  SerialStream_EnableDebuggerInput
#endif /*OPTION_DDB*/
};
