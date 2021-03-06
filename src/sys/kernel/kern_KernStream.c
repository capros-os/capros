/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2008, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>

#ifdef OPTION_DDB
bool kstream_debuggerIsActive = false;
#endif
bool userUsingConsole = false;

void
kstream_InitStreams()
{
  /* LogStream needs no initialization */
  kstream_ConsoleStream->Init();
#ifdef OPTION_OUTPUT_ON_TTY0
  TheSerialStream.Init();
#endif

  printf("Kernel streams initialized...\n");
}

bool
kstream_IsPrint(uint8_t c)
{
  if (c > 127) {
    return false;
  }
  else {
    static uint8_t isPrint[16] = {	/* really a bitmask! */
      0x00,			/* BEL ACK ENQ EOT ETX STX SOH NUL */
      0x00,			/* SI  SO  CR  FF  VT  LF  TAB BS */
      0x00,			/* ETB SYN NAK DC4 DC3 DC2 DC1 DLE */
      0x00,			/* US  RS  GS  FS  ESC SUB EM  CAN */
      0xff,			/* '   &   %   $   #   "   !   SPC */
      0xff,			/* /   .   -   ,   +   *   )   ( */
      0xff,			/* 7   6   5   4   3   2   1   0 */
      0xff,			/* ?   >   =   <   ;   :   9   8 */
      0xff,			/* G   F   E   D   C   B   A   @ */
      0xff,			/* O   N   M   L   K   J   I   H */
      0xff,			/* W   V   U   T   S   R   Q   P */
      0xff,			/* _   ^   ]   \   [   Z   Y   X */
      0xff,			/* g   f   e   d   c   b   a   ` */
      0xff,			/* o   n   m   l   k   j   i   h */
      0xff,			/* w   v   u   t   s   r   q   p */
      0x7f,			/* DEL ~   }   |   {   z   y   x */
    } ;
    
    uint8_t mask = isPrint[(c >> 3)];
    c &= 0x7u;
    if (c)
      mask >>= c;
    if (mask & 1)
      return true;
    return false;
  }
}

/* Send character to up to 3 destinations:
To Console during boot, and also if debugger is using console.
To Serial if OPTION_OUTPUT_ON_TTY0.
To Log if Debugger not active. */
void
kstream_do_putc(char c)
{
#if (defined(OPTION_DDB) && defined(OPTION_OUTPUT_ON_CONSOLE))
  /* In this case always use the console. */
  kstream_ConsoleStream->Put(c);
#else
  /* Use console until user needs it. */
  if (! userUsingConsole) kstream_ConsoleStream->Put(c);
#endif

#ifdef OPTION_OUTPUT_ON_TTY0
  TheSerialStream.Put(c);
#endif

  /* Non-debugger output goes to log. */
#ifdef OPTION_DDB
  if (!kstream_debuggerIsActive) LogStream_Put(c);
#else
  LogStream_Put(c);
#endif
}

void
kstream_PutBuf(uint8_t *s, uint32_t len)
{
  uint32_t i = 0;

  for (i = 0; i < len; i++)
    kstream_nl_putc(s[i]);
}

void
kstream_nl_putc(char c)
{
  if (c == '\n')
    kstream_do_putc('\r');

  kstream_do_putc(c);
}

#ifdef OPTION_DDB
uint8_t
KernStream_Get()
{
  fatal("Fatal call to KernStream::Get() made.\n");
  return SPC;
}

void
KernStream_SetDebugging(bool onOff)
{
  fatal("Fatal call to KernStream::SetDebugging() made.\n");
}
#endif

/* should be BeginUsingConsole */
void 
kstream_BeginUserActivities()
{
#if (defined(OPTION_DDB) && defined(OPTION_OUTPUT_ON_CONSOLE))
  printf("Starting first thread...\n");
#else
  printf("Starting first thread. Console output now disabled...\n");
#endif
  userUsingConsole = true;
}
