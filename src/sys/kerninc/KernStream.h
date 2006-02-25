#ifndef __KERNSTREAM_H__
#define __KERNSTREAM_H__
/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
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

/** KernStream is really an interface specification. The kernel
 * maintains three KernStreams: ConsoleStream, SerialStream,
 * LogStream. LogStream is global, and present in all
 * kernels. ConsoleStream is present if a console is
 * configured. SerialStream is present if the kernel debugger has been
 * configured for serial mode.
 *
 * Pointers to the ConsoleStream and SerialStream objects are in
 * turn conditionally assigned to two variables syscon_stream and
 * dbg_stream.
 */

enum ASCII {
  NUL = 0x00,
  SOH = 0x01,
  STX = 0x02,
  ETX = 0x03,
  EOT = 0x04,
  ENQ = 0x05,
  ACK = 0x06,
  BEL = 0x07,
  BS  = 0x08,
  TAB = 0x09,
  LF  = 0x0a,
  VT  = 0x0b,
  FF  = 0x0c,
  CR  = 0x0d,
  SO  = 0x0e,
  SI  = 0x0f,
  DLE = 0x10,
  DC1 = 0x11,
  DC2 = 0x12,
  DC3 = 0x13,
  DC4 = 0x14,
  NAK = 0x15,
  SYN = 0x16,
  ETB = 0x17,
  CAN = 0x18,
  EM  = 0x19,
  SUB = 0x1a,
  ESC = 0x1b,
  FS  = 0x1c,
  GS  = 0x1d,
  RS  = 0x1e,
  US  = 0x1f,
  SPC = 0x20,
  DEL = 0x7f,
};

struct KernStream {
  void (*Init)();
  void (*Put)(uint8_t c);

#ifdef OPTION_DDB
  /* Following only called via dbg_stream */
  uint8_t (*Get)();
  void (*SetDebugging)(bool onoff);
  void (*EnableDebuggerInput)();
#endif
};

typedef struct KernStream KernStream;

/* Former static data members of KernStream */

#ifdef OPTION_DDB
extern bool kstream_debuggerIsActive;
#else
#define kstream_debuggerIsActive 0
#endif

extern void LogStream_Put(uint8_t c);
extern KernStream* kstream_ConsoleStream;

#if defined(OPTION_OUTPUT_ON_CONSOLE)
#define kstream_dbg_stream kstream_ConsoleStream
#elif defined(OPTION_OUTPUT_ON_TTY0)
#define kstream_dbg_stream kstream_SerialStream
extern KernStream * kstream_SerialStream;
#endif

void kstream_PutBuf(uint8_t *s, uint32_t len);

bool kstream_IsPrint(uint8_t c);

void kstream_InitStreams();

void kstream_do_putc(char c);

void kstream_nl_putc(char c);

void kstream_BeginUserActivities();

#endif /* __KERNSTREAM_H__ */
