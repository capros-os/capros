#ifndef __ANSI_H__
#define __ANSI_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#include <graphics/color.h>

/* FIX: Don't we have one approved, public definition for min and max? */
#ifndef min
  #define min(a,b) ((a) <= (b) ? (a) : (b))
#endif

#ifndef max
  #define max(a,b) ((a) >= (b) ? (a) : (b))
#endif

#define OFF false
#define ON  true

#define MaxParam 2

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

typedef enum State State;
enum State {
  NotInit,
  WantChar,
  SawEsc,
  WantParam,
};

typedef enum Attr Attr;
enum Attr {
  Normal,
  Bold,
  Underline,
  Blink,
  Reverse,
  Invisible,
};

typedef struct AnsiTerm AnsiTerm;
struct AnsiTerm {
  bool scroll;
  bool blinkCursor;
  State state;
  uint32_t pos;
  uint32_t maxpos;
  uint32_t cols;
  uint32_t rows;
  color_t fg;
  color_t bg;
  Attr curAttr;
  uint16_t cursor;
  uint32_t nParams;
  uint32_t param[MaxParam];
};

#define EA_NORMAL          0x00u
#define EA_WITH_NEW_ATTRS  0x01u

void ansi_reset(AnsiTerm *at, uint32_t row, uint32_t cols);
void ansi_put(AnsiTerm *at, uint16_t c);
void ansi_set_cursor(AnsiTerm *at, uint16_t glyph);
void ansi_retype_all(AnsiTerm *at, uint32_t flag);
void ansi_resize(AnsiTerm *at, uint32_t new_rows, uint32_t new_cols);
void ansi_clear_screen(AnsiTerm *at);
void ansi_make_blink(AnsiTerm *at, bool on);
void ansi_set_cursor_blink(AnsiTerm *at, bool on);

#endif
