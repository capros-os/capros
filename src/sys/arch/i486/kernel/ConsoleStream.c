/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group.
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

/* This is a (poor) implementation of a text-mode kernel frame buffer
 * for use in diagnostics and debugger output. It assumes that the
 * display has been left in the standard VGA text mode by the
 * bootstrap code. */

#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <kerninc/Machine.h>
#include <eros/arch/i486/io.h>
#include "IDT.h"
#include "IRQ386.h"

#define SCREEN_START PTOV(0xb8000u)
#define SCREEN_ROWS 25
#define SCREEN_COLS 80
#define SCREEN_END (SCREEN_START + 2*SCREEN_COLS*SCREEN_ROWS)

static void ClearTextConsole();

static unsigned long offset;
  
static uint32_t StartAddressReg = 0;	/* as seen by the display hardware */
static uint16_t *screen = (uint16_t *) SCREEN_START;

void
ConsoleStream_Init()
{
  uint8_t hi, lo;

  outb(0xc, 0x3d4);		/* start address hi register addr */
  hi = inb(0x3d5);
  outb(0xd, 0x3d4);		/* start address lo register addr */
  lo = inb(0x3d5);

  StartAddressReg = hi << 8 | lo;

  offset = 0;

  ClearTextConsole();
}

enum VGAColors {
  Black = 0,
  Blue = 1,
  Green = 2,
  Cyan = 3,
  Red = 4,
  Magenta = 5,
  Brown = 6,
  White = 7,
  Gray = 8,
  LightBlue = 9,
  LightGreen = 10,
  LightCyan = 11,
  LightRed = 12,
  LightMagenta = 13,
  LightBrown = 14,	/* yellow */
  BrightWhite = 15,

  /* Combinations used by the console driver: */
  WhiteOnBlack = 0x7,
  blank = 0,
};

static void
SetPosition(uint32_t pos, uint8_t c)
{
  uint16_t vgaAttrs = WhiteOnBlack << 8;

  screen[pos] = ((uint16_t) c) | vgaAttrs;
}

static void
ShowCursorAt(uint32_t pos)
{
  uint32_t cursAddr = (uint32_t) pos;

  cursAddr += StartAddressReg;

  outb(0xE, 0x3D4);
  outb((cursAddr >> 8) & 0xFFu, 0x3D5);
  outb(0xF, 0x3D4);
  outb((cursAddr & 0xFFu), 0x3D5);
}

static void
Scroll(uint32_t startPos, uint32_t endPos, int amount)
{
  uint32_t p = 0;

  if (amount > 0) {
    uint32_t gap = amount;
    for (p = startPos + gap; p < endPos; p++)
      screen[p] = screen[p - gap];

    for (p = startPos; p < startPos + gap; p++)
      screen[p] = (WhiteOnBlack << 8);
  }
  else {
    uint32_t gap = -amount;

    for (p = startPos; p < endPos - gap; p++)
      screen[p] = screen[p + gap];

    for (p = endPos - gap; p < endPos; p++)
      screen[p] = (WhiteOnBlack << 8);
  }
}

static void
ClearTextConsole()
{
  uint32_t wpos = 0;

  for (wpos = 0; wpos < SCREEN_ROWS * SCREEN_COLS; wpos++)
    SetPosition(wpos, ' ');
}


static void Beep() {}

/* FIX: This is NOT RIGHT!! */
void
ConsoleStream_Put(uint8_t c)
{
  const unsigned cols = SCREEN_COLS;
  const unsigned rows = SCREEN_ROWS;

  const int TABSTOP = 8;
  uint32_t posCol = offset % cols;

  /* On newline, clear to EOL: */
  if (c == CR)
    if (offset % cols) Scroll (offset, offset + (cols - posCol), (cols - posCol));

  if (kstream_IsPrint(c)) {
    SetPosition(offset, c);
    offset++;
  }
  else {
    /* Handle the non-printing characters: */
    switch(c) {
    case BEL:
      Beep();
      break;
    case BS:		/* backspace is NONDESTRUCTIVE */
      if ( offset % cols )
	offset--;
      break;
    case TAB:		/* NONDESTRUCTIVE until we know how */
      while (offset % TABSTOP) {
	SetPosition(offset, ' ');
	offset++;
      }
      break;
    case LF:
      offset += cols;
      break;
    case VT:		/* reverse line feed */
      if (offset > cols)
	offset -= cols;
      break;
#if 0
    case ASCII::FF:		/* reverse line feed */
      offset = 0;
      ClearScreen();
      break;
#endif
    case CR:
      offset -= (offset % cols);
      break;
    }
  }
    
  if (offset >= rows * cols) {
    Scroll(0, rows * cols, - (int) cols);
    offset -= cols;
  }

  assert (offset < rows * cols);
  ShowCursorAt(offset);
  return;
}

/*************************************************************************
 *
 * EVERYTHING FROM HERE DOWN IS KEYBOARD DRIVER!!!!
 *
 * The keyboard logic is enabled only if the debugger is running on
 * the console.
 *
 *************************************************************************/

#if (defined(OPTION_DDB) && defined(OPTION_OUTPUT_ON_CONSOLE))

const uint8_t KbdDataPort    = 0x60u;
const uint8_t KbdCtrlPort   = 0x64u;
const uint8_t KbdStatusPort = 0x64u;


enum KeyCmd {
  SetLed = 0xedu,
} ;



enum KbdStatus{
  kbd_BufFull = 0x1,
  kbd_Ready   = 0x2,
};



enum KeyMod {
  /* Note that the first three values correspond to the bitmask for the
   * keyboard LED's -- this is not an accident!
   */
  ScrlLock  = 0x01u,
  NumLock   = 0x02u,
  AlphaLock = 0x04u,
  
  Shift     = 0x10u,
  Ctrl      = 0x20u,
  Alt       = 0x40u,
  
  
  Extended  = 0x100u,		/* key is an "extended" key */
  IsAlpha   = 0x200u,		/* key is modified by alpha lock key */
  IsPad     = 0x400u,		/* key is modified by num lock key */
  Meta      = 0x800u,		/* key can be meta'd */
} ;


static uint32_t ShiftState = 0;

/* kbd_wait -- wait for a character to be available from the keyboard. */
static void 
KbdWait(void)
{
  int i = 100;

  while (i--) {
    if ((inb(KbdStatusPort) & kbd_Ready) == 0) 
      break;
    SpinWaitUs(10);
  }

#if 0
  printf("KbdWait fails\n");
#endif
}

static void 
KbdCmd(uint8_t command)
{
  int retry = 5;
  do {
    int i = 100000;

    KbdWait();

    outb(command, KbdDataPort);

    while (i--) {
      if (inb(KbdStatusPort) & kbd_BufFull) {
	int val;
	/* DELAY(10); */
	val = inb(KbdDataPort);
	if (val == 0xfa)
	  return;
	if (val == 0xfe)
	  break;
      }
    }

  } while (retry--);
  printf("KbdCmd fails\n");
}

static bool
ReadKbd(uint8_t* c /*@ not null @*/)
{
  KbdWait();

  while ( (inb(KbdStatusPort) & kbd_BufFull) == 0 )
    return false;
  
  *c = inb(KbdDataPort);
  return true;
}

static void
UpdateKbdLeds()
{
  KbdCmd(SetLed);
  KbdCmd(ShiftState & 0x7u);
}
	       
/* Keyboard interpretation proceeds in two phases.  First, the scan
 * code is converted into a virtual key code, performing any necessary
 * keyboard escape translation.  Then the key code translation table
 * is consulted to decide what character to return and whether to
 * update the shift state (if at all).
 */

#define NOP 256

#define NOCHAR(name) {{ NOP, NOP, NOP, NOP },   0 }
#define ALPHA(X) {{ X+32, X, X - 64, X - 64 }, Meta|IsAlpha }
#define KEY(X, Y) {{ X, Y, X, Y },   Meta }
#define PAD(X, Y) {{ X, Y, X, Y },   IsPad }
#define F(X) (X + 256)

#define num_scan 0x59

struct KeyInfo {
  uint16_t value[4];		/* base, shift, ctrl, shift-ctrl */
  uint16_t flags;
} key_table[num_scan] = {
  NOCHAR(None),			/* 0x00 */
  KEY('\027', '\027'),		/* 0x01 */
  KEY('1', '!'),		/* 0x02 */
  { { '2', '@', '\0', '\0'}, 0 }, /* 0x03 -- generate NUL */
  KEY('3', '#'),		/* 0x04 */
  KEY('4', '$'),		/* 0x05 */
  KEY('5', '%'),		/* 0x06 */
  KEY('6', '^'),		/* 0x07 */
  KEY('7', '&'),		/* 0x08 */
  KEY('8', '*'),		/* 0x09 */
  KEY('9', '('),		/* 0x0a */
  KEY('0', ')'),		/* 0x0b */
  KEY('-',  '_'),		/* 0x0c */
  KEY('=',  '+'),		/* 0x0d */
  KEY(0x08, 0x08),		/* 0x0e */
  KEY(0x09, 0x08),		/* 0x0f -- is back tab right? */
  ALPHA('Q'),			/* 0x10 */
  ALPHA('W'),			/* 0x11 */
  ALPHA('E'),			/* 0x12 */
  ALPHA('R'),			/* 0x13 */
  ALPHA('T'),			/* 0x14 */
  ALPHA('Y'),			/* 0x15 */
  ALPHA('U'),			/* 0x16 */
  ALPHA('I'),			/* 0x17 */
  ALPHA('O'),			/* 0x18 */
  ALPHA('P'),			/* 0x19 */
  KEY('[',  '{'),		/* 0x1a */
  KEY(']',  '}'),		/* 0x1b */
  KEY('\r',  '\r'),		/* 0x1c -- enter */
  { { NOP, NOP, NOP, NOP }, Ctrl }, /* 0x1d -- lctrl */
  ALPHA('A'),			/* 0x1e */
  ALPHA('S'),			/* 0x1f */
  ALPHA('D'),			/* 0x20 */
  ALPHA('F'),			/* 0x21 */
  ALPHA('G'),			/* 0x22 */
  ALPHA('H'),			/* 0x23 */
  ALPHA('J'),			/* 0x24 */
  ALPHA('K'),			/* 0x25 */
  ALPHA('L'),			/* 0x26 */
  KEY(';',  ':'),		/* 0x27 */
  KEY('\'', '"'),		/* 0x28 */
  KEY('`',  '~'),		/* 0x29 */
  { { NOP, NOP, NOP, NOP }, Shift }, /* 0x2a -- lshift */
  KEY('\\', '|'),		/* 0x2b */
  ALPHA('Z'),			/* 0x2c */
  ALPHA('X'),			/* 0x2d */
  ALPHA('C'),			/* 0x2e */
  ALPHA('V'),			/* 0x2f */
  ALPHA('B'),			/* 0x30 */
  ALPHA('N'),			/* 0x31 */
  ALPHA('M'),			/* 0x32 */
  KEY(',',  '<'),		/* 0x33 */
  KEY('.',  '>'),		/* 0x34 */
  KEY('/',  '?'),		/* 0x35 */
  { { NOP, NOP, NOP, NOP }, Shift }, /* 0x36 -- rshift */
  KEY('*',  '*'),		/* 0x37 */
  { { NOP, NOP, NOP, NOP }, Alt }, /* 0x38 -- lalt */
  KEY(' ',  ' '),		/* 0x39 -- space */
  { { NOP, NOP, NOP, NOP }, AlphaLock }, /* 0x3a -- alpha lock */
  KEY( F(1), NOP ),		/* 0x3b -- F1 */
  KEY( F(2), NOP ),		/* 0x3c -- F2 */
  KEY( F(3), NOP ),		/* 0x3d -- F3 */
  KEY( F(4), NOP ),		/* 0x3e -- F4 */
  KEY( F(5), NOP ),		/* 0x3f -- F5 */
  KEY( F(6), NOP ),		/* 0x40 -- F6 */
  KEY( F(7), NOP ),		/* 0x41 -- F7 */
  KEY( F(8), NOP ),		/* 0x42 -- F8 */
  KEY( F(9), NOP ),		/* 0x43 -- F9 */
  KEY( F(10), NOP ),		/* 0x44 -- F10 */
  { { NOP, NOP, NOP, NOP }, NumLock }, /* 0x45 -- num lock */
  { { NOP, NOP, NOP, NOP }, ScrlLock }, /* 0x46 -- scroll-lock */

  /* Keypad character mappings -- these assume that num-lock is NOT set! */
  PAD( F(15), '7' ),		/* 0x47 -- keypad 7 */
  PAD( F(16), '8' ),		/* 0x48 -- keypad 8 */
  PAD( F(17), '9' ),		/* 0x49 -- keypad 9 */
  KEY( '-',   NOP ),		/* 0x4a -- keypad - */
  PAD( F(19), '4' ),		/* 0x4b -- keypad 4 */
  PAD( NOP,   '5' ),		/* 0x4c -- keypad 5 */
  PAD( F(20), '6' ),		/* 0x4d -- keypad 6 */
  KEY( '+',   '+' ),		/* 0x4e -- keypad + */
  PAD( F(22), '1' ),		/* 0x4f -- keypad 1 */
  PAD( F(23), '2' ),		/* 0x50 -- keypad 2 */
  PAD( F(24), '3' ),		/* 0x51 -- keypad 3 */
  PAD( F(25), '0' ),		/* 0x52 -- keypad 0 */
  PAD( 0x7f,  '.' ),		/* 0x53 -- keypad ./DEL */
  KEY( NOP, NOP ),		/* 0x54 -- unused! */
  KEY( NOP, NOP ),		/* 0x55 -- unused! */
  KEY( NOP, NOP ),		/* 0x56 -- unused! */
  KEY( F(11), NOP ),		/* 0x57 -- F11 */
  KEY( F(12), NOP ),		/* 0x58 -- F12 */
#if 0
  /* CHARACTERS BELOW THIS POINT ARE RECODED!!! */

  { { NOP, NOP, NOP, NOP }, KeyMod::Ctrl }, /* 0x59 rctrl */
  { { NOP, NOP, NOP, NOP }, KeyMod::Alt }, /* 0x5a ralt */

e0,1c	kpd-enter
e0,1d	rctrl		SUPPRESSED
e0,35	kpd-/
e0,37	print-screen
e0,38   ralt		SUPPRESSED
e0,47	home		
e0,48	uparrow
e0,49	PgUp
e0,4b	left-arrow		
e0,4d	right-arrow
e0,4f	end		
e0,50	downarrow
e0,51	PgDn
e0,52	insert		
e0,53	delete		
e0,5b	lwindow
e0,5c	rwindow
e0,5d	menu
e1,1d,68 pause
#endif
};

typedef struct KeyInfo KeyInfo;

static uint32_t
FetchInputFromKeyboard()
{
  static uint8_t esc_code = 0;
  KeyInfo *ki = 0;
  uint32_t ndx = 0;
  uint32_t ascii = 0;

  uint8_t scanCode = 0;
  
  while ( ReadKbd(&scanCode) ) {
    /* printf("<kc = %x>", scanCode); */
    bool shift = BOOL(ShiftState & Shift);
#if 0
    bool alt = ShiftState & KeyMod::Alt;
#endif
    bool ctrl = BOOL(ShiftState & Ctrl);
      
    /* If this is a break character, we need to know: */
    bool isBreak = BOOL(scanCode & 0x80u);

    uint32_t keyCode = scanCode & 0x7fu;
      
    switch (esc_code) {
    case 0x0:
      {
	/* printf("esc_code==0\n"); */
	switch (scanCode) {
	case 0xe0:
	case 0xe1:
	  esc_code = scanCode;
	  continue;
	default:
	  if (keyCode >= num_scan) {
	    printf("<? 0x0 \\x%x>", keyCode);
	    return NOP;
	  }
	}
	break;
      }
    case 0xe0:
      {
	switch (keyCode) {
	case 0x2a:		/* shift hack used by some keyboards */
				/* for extra keys! */
	  shift = ! shift;

	case 0x1c:		/* kpd-enter */
	case 0x1d:		/* rctrl */
	case 0x35:		/* kpd-/ */
	case 0x38:		/* ralt */
	case 0x47:		/* home */
	case 0x48:		/* uparrow */
	case 0x49:		/* pgup */
	case 0x4b:		/* left-arrow */
	case 0x4d:		/* right-arrow */
	case 0x4f:		/* end */
	case 0x50:		/* down-arrow */
	case 0x51:		/* pgdn */
	case 0x52:		/* insert */
	case 0x53:		/* del */
	  esc_code = 0;
	  break;

	case 0x5b:		/* lwindow */
	case 0x5c:		/* rwindow */
	case 0x5d:		/* menu */
	  /* consume these transparently: */
	  esc_code = 0;
	  return NOP;
	default:
	  printf("<? 0xe0 \\x%x>", scanCode);
	  esc_code = 0;
	  return NOP;
	}
	break;
      }

    case 0xe1:
      {
	if (keyCode == 0x1d) {
	  esc_code = scanCode;
	  continue;
	}
	else {
	  esc_code = 0;
	  printf("<? 0xe1 \\x%x>", scanCode);
	  return NOP;
	}
	break;
      }
    case 0x1d:
      {
	if (keyCode == 0x68) {
	  /* consume transparently */
	  esc_code = 0;
	  return NOP;
	}
	else {
	  printf("<? 0x1d \\x%x>", scanCode);
	  esc_code = 0;
	  return NOP;
	}
	break;
      }
    default:
      printf("Unknown escape 0x%x\n", esc_code); 
      break;
    }
      
    /* printf("Key code is %d (0x%x)\n", keyCode, keyCode);  */
    ki /*@ not null @*/ = &key_table[keyCode];

    /* printf("<kf=\\x%x,0x%x>", keyCode, ki.flags); */

    if ( (ki->flags & IsAlpha) &&
	 (ShiftState & AlphaLock) )
      shift = !shift;

    if ( (ki->flags & IsPad) &&
	 (ShiftState & NumLock) )
      shift = !shift;

    ndx = (shift ? 1 : 0) + (ctrl ? 2 : 0);
    ascii = ki->value[ndx];

      /* keep track of shift, ctrl, alt */
    if (isBreak)
      ShiftState &= ~ (ki->flags & 0xf0u);
    else {
      ShiftState |= (ki->flags & 0xf0u);
    }

    /* keep track of the various lock keys on the break, not the
       * keypress - the break doesn't repeat.  These are toggles, thus
       * the XOR:
       */

    if (isBreak && (ki->flags & 0xfu)) {
      ShiftState ^= (ki->flags & 0xfu);
      UpdateKbdLeds();
    }

    if (isBreak || ascii >= NOP)
      return NOP;

#if 0
      /* Check for three-fingered salute: */
    if ( keyCode == 0x53u && ctrl && alt)
      Reboot();
#endif

#if 0
#ifdef OPTION_DDB
    /* Check for kernel debugger: */
    if ( keyCode == 0x20u && ctrl && alt)/* 20u == 'd' */
      Debugger();

    if ( keyCode == 0x2eu && ctrl ) /* 1fu == 'c' */
      Debugger();
#endif
#endif

#ifdef CORNER_HACK
    *((uint16_t *) 0x000b8000) = 0x8f00 | (ascii & 0xffu);
#endif
    return ascii;
  }

  return NOP;
}

static void
KeyboardInterrupt(savearea_t *sa)
{
  uint32_t c;
  uint32_t irq = IRQ_FROM_EXCEPTION(sa->ExceptNo);

  assert(irq == 1);

  if (kstream_debuggerIsActive)
    return;

#ifdef CORNER_HACK
  *((uint16_t *) 0x000b8000) = 0x8f00 | 'I';
#endif
  c = FetchInputFromKeyboard();
  if (c == ETX)
    Debugger();

  irq_Enable(irq);
}

uint8_t
ConsoleStream_Get()
{
  uint32_t c;

#ifdef CORNER_HACK
  *((uint16_t *) 0x000b8000) = 0x8f00 | 'P';
#endif

  do {
    c = FetchInputFromKeyboard();
  } while (c == NOP);

#ifdef CORNER_HACK
  *((uint16_t *) 0x000b8000) = 0x8f00 | 'G';
#endif

  return c;
}

void
ConsoleStream_SetDebugging(bool onOff)
{
  kstream_debuggerIsActive = onOff;
  
  if (kstream_debuggerIsActive == false)
    irq_Enable(irq_Keyboard);
}

void
ConsoleStream_EnableDebuggerInput()
{
  irq_SetHandler(irq_Keyboard, KeyboardInterrupt);
  printf("Set up keyboard (console) interrupt handler!\n");

#if 0
  /* Establish initial keyboard state visibly: */
  UpdateKbdLeds();
#endif

  /* FIX: This may prove a problem, as I need to check exactly where
   * the interrupt vectors are set up in the boot sequence... */
  irq_Enable(irq_Keyboard);
}

#elif defined(OPTION_DDB)
void
ConsoleStream_EnableDebuggerInput()
{
  fatal("EnableDebuggerInput on console when not OUTPUT_ON_CONSOLE\n");
}

void
ConsoleStream_SetDebugging(bool onOff)
{
  fatal("SetDebugging() on console when not OUTPUT_ON_CONSOLE\n");
}


uint8_t
ConsoleStream_Get()
{
  fatal("Get() on console when not OUTPUT_ON_CONSOLE\n");
  return 0;
}

#endif /* OPTION_OUTPUT_ON_CONSOLE */

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
