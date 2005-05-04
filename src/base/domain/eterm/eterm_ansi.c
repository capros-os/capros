/*
 * copyright (C) 2003, Jonathan S. Shapiro.
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

#include <eros/target.h>

#include "eterm_ansi.h"

#include <stdlib.h>
#include <ctype.h>

#include <domain/domdbg.h>

typedef struct Cell Cell;
struct Cell {
  bool useDefFG;
  bool useDefBG;
  bool useDefAttr;
  uint16_t val;
  color_t fg;
  color_t bg;
  Attr attr;
};

/* globals */
#define MAXPOS (50000)
static Cell screen[MAXPOS];
static uint32_t prevCurs = 0;	/* cursor position */
static color_t default_fg = BLACK;
static color_t default_bg = BLACK;
static bool useDefFG   = true;
static bool useDefBG   = true;
static bool useDefAttr = true;
static bool blinkstate = ON;

/* defined in eterm_out.c */
extern void eterm_echo(AnsiTerm *at, uint32_t pos, uint16_t c, 
		       bool withUpdate);
extern void eterm_echo_cursor(AnsiTerm *at, uint32_t pos, bool withUpdate);
extern void eterm_update(AnsiTerm *at, uint32_t startPos, uint32_t endPos);

extern cap_t kr_ostream;

static void
SetPosition(AnsiTerm *at, uint16_t c, bool forward)
{
  if (at == NULL)
    return;

  screen[at->pos].val = c;
  screen[at->pos].fg = at->fg;
  screen[at->pos].bg = at->bg;
  screen[at->pos].attr = at->curAttr;
  screen[at->pos].useDefFG = useDefFG;
  screen[at->pos].useDefBG = useDefBG;
  screen[at->pos].useDefAttr = useDefAttr;

  eterm_echo(at, at->pos, c, true);

  at->maxpos = max(at->pos, at->maxpos);

  /* Advance cursor pos */
  if (forward)
    at->pos++;
}

static void
ShowCursor(AnsiTerm *at)
{
  if (at == NULL)
    return;

  if (!at->blinkCursor || blinkstate == ON)
    eterm_echo_cursor(at, at->pos, true);

  prevCurs = at->pos;
}

static void
Scroll(AnsiTerm *at, uint32_t startPos, uint32_t endPos, int amount)
{
  AnsiTerm tmp;

  if (at == NULL)
    return;

  tmp = *at;

  if (amount > 0) {
    uint32_t gap = amount;
    uint32_t p = endPos;

    while (p >= startPos) {
      screen[p] = screen[p - gap];
      tmp.fg = screen[p].fg;
      tmp.bg = screen[p].bg;
      tmp.curAttr = screen[p].attr;
      eterm_echo(&tmp, p, screen[p].val, false);
      p--;
    }

    /* Fill in holes with blanks */
    for (p = startPos; p < startPos + gap; p++) {
      screen[p].val = ' ';
      screen[p].fg = at->fg;
      screen[p].bg = at->bg;
      screen[p].attr = Normal;
      screen[p].useDefFG = true;
      screen[p].useDefBG = true;
      screen[p].useDefAttr = true;
      eterm_echo(at, p, screen[p].val, false);
    }
  }
  else {
    uint32_t gap = -amount;
    uint32_t p = startPos;

    while (p <= endPos) {
      screen[p] = screen[p + gap];
      tmp.fg = screen[p].fg;
      tmp.bg = screen[p].bg;
      tmp.curAttr = screen[p].attr;
      eterm_echo(&tmp, p, screen[p].val, false);
      p++;
    }

    /* Fill in holes with blanks */
    for (p = endPos - gap; p <= endPos; p++) {
      screen[p].val = ' ';
      screen[p].fg = at->fg;
      screen[p].bg = at->bg;
      screen[p].attr = Normal;
      screen[p].useDefFG = true;
      screen[p].useDefBG = true;
      screen[p].useDefAttr = true;
      eterm_echo(at, p, screen[p].val, false);
    }
  }

  /* Request an update of the scrolled region */
  eterm_update(at, startPos, endPos);
}

static void
scroll_if_needed(AnsiTerm *at)
{
  if (at == NULL)
    return;
    
  if (at->pos >= at->rows * at->cols) {
    if (at->scroll) {
      Scroll(at, 0, at->rows * at->cols, - (int) at->cols);
      at->pos -= at->cols;
    }
    else
      at->pos = (at->pos % at->cols);
  }
}

static void
Beep()
{
}

static void
ProcessChar(AnsiTerm *at, uint8_t b)
{
  const int TABSTOP = 8;
  
  if (at == NULL)
    return;

  if ((isprint)(b))
    SetPosition(at, b, true);
  else {
    /* Handle the non-printing characters: */
    switch(b) {
    case BEL:
      Beep();
      break;
    case BS:
      {
	if (blinkstate != ON)
	  kprintf(kr_ostream, "**ERR etermAnsi: BS blinkstate not ON\n");

	eterm_echo_cursor(at, prevCurs, true);

	/* FIX: we really need to scroll everything left on a BS */
	at->pos--;
	SetPosition(at, ' ', false);
      }
      break;

    case TAB:		
      /* Deal with case where cursor is sitting on a tab stop: */
      if ((at->pos % TABSTOP) == 0)
	SetPosition(at, ' ', true);

      while (at->pos % TABSTOP)
	SetPosition(at, ' ', true);
      break;
    case LF:
      /* First erase old cursor */
	if (blinkstate != ON)
	  kprintf(kr_ostream, "**ERR etermAnsi: LF blinkstate not ON\n");

	eterm_echo_cursor(at, prevCurs, true);

      /* Then, update pos */
      at->pos += at->cols;
      break;
    case VT:		/* reverse line feed */
      if (at->pos > at->cols)
	at->pos -= at->cols;
      break;
#if 0
    case FF:		/* reverse line feed */
      at->pos = 0;
      ClearScreen();
      break;
#endif
    case CR:
      /* First erase old cursor */
	if (blinkstate != ON)
	  kprintf(kr_ostream, "**ERR etermAnsi: CR blinkstate not ON\n");

	eterm_echo_cursor(at, prevCurs, true);

      /* Then, update pos */
      at->pos -= (at->pos % at->cols);
      break;
    }
  }

  scroll_if_needed(at);

  if (at->pos < at->rows * at->cols)
    ShowCursor(at);

  return;
}

void
ProcessEsc(AnsiTerm *at, uint8_t b)
{
  uint32_t posRow;
  uint32_t posCol;
  //  uint32_t p;

  if (at == NULL)
    return;

  posRow = at->pos / at->cols;
  posCol = at->pos % at->cols;

#if 0
  /* I don't yet wish to support this. */
  
  /* The ESC [ N 'm' sequence is the only one that has a default
   * parameter value of zero.  Handle that one specially so we can do
   * common logic on the rest:
   */

  if (b == 'm') {
    if (at->nParams != 1)
      return;
    switch(at->param[0]) {
    case 0:
    case 7:
      break;
    }
  }
  
  /* Do default handling: */
  for (p = 0; p < MaxParam; p++) {
    if (at->param[p] == 0)
      at->param[p] = 1;
  }
#endif
  
  switch(b) {
  case '@':			/* insert N characters */
    {
      int distance = min(at->param[0], at->cols - posCol);

      Scroll(at, at->pos, at->pos + (at->cols * at->rows), distance);
      break;
    }
  case 'A':			/* Cursor Up N */
    {
      uint32_t count = min(at->param[0], posRow);
      at->pos -= (at->cols * count);
      break;
    }
  case 'B':			/* Cursor Down N */
    {
      uint32_t count = min(at->param[0], at->rows - posRow - 1);
      at->pos += (at->cols * count);
      break;
    }
  case 'C':			/* Cursor Forward N */
    {
      uint32_t count = at->param[0];
      at->pos += count;
      break;
    }
  case 'D':			/* Cursor Back N */
    {
      uint32_t count = at->param[0];
      at->pos -= count;
      break;
    }
  case 'E':			/* Cursor Start of N Lines Down (N) */
    {
      uint32_t count = min(at->param[0], at->cols - posCol - 1);
      at->pos += count;
      at->pos -= (at->pos % at->cols);
      break;
    }
  case 'f':			/* Cursor position (row, col) */
  case 'H':			/* Cursor position (row, col) */
    {
      uint32_t therow = min(at->param[0], at->rows);
      uint32_t thecol = min(at->param[1], at->cols);

      therow--;
      thecol--;

      at->pos = (therow * at->cols) + thecol;

      break;
    }
  case 'J':			/* Erase Screen and Position cursor at
				   (0,0)*/
    {
      if (at->param[0] == 2) {

	/* Use Scroll to clear the screen */
      	Scroll(at, 0, (at->rows*at->cols), - (at->rows * at->cols));

	/* Toggle display of the cursor */
	eterm_echo_cursor(at, prevCurs, false);

	/* Reposition cursor */
	at->pos = 0;
      }
      break;
    }
  case 'K':			/* Erase to End of Line (none) */
    {
      Scroll(at, at->pos, at->pos + (at->cols - posCol), (at->cols - posCol));
      break;
    }
  case 'L':			/* Insert N Lines above current */
    {
      uint32_t nRows = min(at->param[0], at->rows - posRow);
      Scroll(at, posRow * at->cols, at->rows * at->cols, nRows * at->cols);
      break;
    }
  case 'M':			/* Delete N Lines from current */
    {
      uint32_t nRows = min(at->param[0], at->rows - posRow);
      Scroll(at, posRow * at->cols, at->rows * at->cols, - ((int) nRows * at->cols));
      break;
    }
  case 'P':			/* Delete N Characters */
    {
      int distance = min(at->param[0], at->cols - posCol);

      Scroll(at, at->pos, at->pos + (at->cols * at->rows), -distance);
      break;
    }
  case 'm':			/* select graphic rendition */
    {
      uint32_t u;

      if (at->nParams == 0)
	break;

      for (u = 0; u < at->nParams; u++)
      switch(at->param[u]) {
      case 0:
	{
	  at->fg = default_fg;
	  at->bg = default_bg;
	  at->curAttr = Normal;
	  useDefFG = true;
	  useDefBG = true;
	  useDefAttr = true;
	  break;
	}
      case 1:
	{
	  at->curAttr = Bold;
	  useDefAttr = false;
	  break;
	}
      case 4:
	{
	  at->curAttr = Underline;
	  useDefAttr = false;
	  break;
	}
      case 5:
	{
	  at->curAttr = Blink;
	  useDefAttr = false;
	  break;
	}
      case 7:
	{
	  at->curAttr = Reverse;
	  useDefAttr = false;
	  break;
	}
      case 8:
	{
	  at->curAttr = Invisible;
	  useDefAttr = false;
	  break;
	}
      case 30:
	{
	  at->fg = BLACK;
	  useDefFG = false;
	  break;
	}
      case 31:
	{
	  at->fg = RED;
	  useDefFG = false;
	  break;
	}
      case 32:
	{
	  at->fg = GREEN;
	  useDefFG = false;
	  break;
	}
      case 33:
	{
	  at->fg = YELLOW;
	  useDefFG = false;
	  break;
	}
      case 34:
	{
	  at->fg = BLUE;
	  useDefFG = false;
	  break;
	}
      case 35:
	{
	  at->fg = MAGENTA;
	  useDefFG = false;
	  break;
	}
      case 36:
	{
	  at->fg = CYAN;
	  useDefFG = false;
	  break;
	}
      case 37:
	{
	  at->fg = WHITE;
	  useDefFG = false;
	  break;
	}
      case 40:
	{
	  at->bg = BLACK;
	  useDefBG = false;
	  break;
	}
      case 41:
	{
	  at->bg = RED;
	  useDefBG = false;
	  break;
	}
      case 42:
	{
	  at->bg = GREEN;
	  useDefBG = false;
	  break;
	}
      case 43:
	{
	  at->bg = YELLOW;
	  useDefBG = false;
	  break;
	}
      case 44:
	{
	  at->bg = BLUE;
	  useDefBG = false;
	  break;
	}
      case 45:
	{
	  at->bg = MAGENTA;
	  useDefBG = false;
	  break;
	}
      case 46:
	{
	  at->bg = CYAN;
	  useDefBG = false;
	  break;
	}
      case 47:
	{
	  at->bg = WHITE;
	  useDefBG = false;
	  break;
	}
      default:
	break;
      }
      break;
    }

#if 0
  case 'p':			/* Set black on white mode */
  case 'q':			/* Set white on black mode */
#endif
  case 's':			/* RESET (none) */
    {
      at->pos = 0;
      at->curAttr = Normal;
      at->state = WantChar;
      break;
    }
  default:
    break;
  }

  at->state = WantChar;
}

void 
ansi_reset(AnsiTerm *at, uint32_t rows, uint32_t cols)
{
  if (at == NULL)
    return;

  at->state = WantChar;
  at->pos = at->maxpos = 0;
  at->fg = BLACK;
  at->bg = 0x00F5DEB3;		/* per shap's request */
  at->curAttr = Normal;
  at->scroll = true;
  at->blinkCursor = true;
  at->rows = rows;
  at->cols = cols;
  default_fg = at->fg;
  default_bg = at->bg;

  ansi_clear_screen(at);
}

void 
ansi_put(AnsiTerm *at, uint16_t c)
{
  if (at == NULL)
    return;

  if (at->state == NotInit)
    return;
  
  switch (at->state) {
  case WantChar:
    {
      if (c == ESC) {
	at->state = SawEsc;
	return;
      }

      ProcessChar(at, c);
      break;
    }
  case SawEsc:
    {
      int i;

      if (c == '[') {
	at->state = WantParam;

	at->nParams = 0;
	for (i = 0; i < MaxParam; i++)
	  at->param[i] = 0;
	
	return;
      }

      /* Escape sequence error -- pretend we never saw it: */
      at->state = WantChar;
      break;
    }
  case WantParam:
    {
      if (at->nParams < 2 && c >= '0' && c <= '9') {
	at->param[at->nParams] *= 10;
	at->param[at->nParams] += (c - '0');
      }
      else if (at->nParams < 2 && c == ';')
	at->nParams++;
      else if (at->nParams == 2)
	at->state = WantChar;
      else {
	at->nParams++;
	ProcessEsc(at, c);
      }
    }
    break;

  default:
    break;
  }
  /* First erase old cursor */
  if (blinkstate != ON)
    kprintf(kr_ostream, "**ERR etermAnsi: ESC blinkstate not ON\n");

  eterm_echo_cursor(at, prevCurs, true);

  ShowCursor(at);
}

void
ansi_set_cursor(AnsiTerm *at, uint16_t glyph)
{
  if (at == NULL)
    return;

  at->cursor = glyph;
}

void
ansi_make_blink(AnsiTerm *at, bool on)
{
  uint32_t p;

  if (at == NULL)
    return;

  for (p = 0; p <= at->maxpos; p++) {
    if (screen[p].attr == Blink) {
      if (on)
	eterm_echo(at, p, screen[p].val, true);
      else
	eterm_echo(at, p, ' ', true);
    }
  }

  if (at->blinkCursor) {
    eterm_echo_cursor(at, at->pos, true);
    blinkstate = on;
  }
}

void
ansi_retype_all(AnsiTerm *at, uint32_t flag)
{
  uint32_t maxp;
  uint32_t p;
  AnsiTerm tmp;

  if (at == NULL)
    return;

  maxp = at->maxpos;

  if (flag & EA_WITH_NEW_ATTRS) {
    default_fg = at->fg;
    default_bg = at->bg;
    maxp = at->rows * at->cols;
  }

  for (p = 0; p <= maxp; p++) {
    tmp = *at;
    tmp.pos = p;

    /* If the updateAttrs flag is set AND the current Cell uses
       defaults then override/change the current Cell's settings */
    if (flag & EA_WITH_NEW_ATTRS) {
      if (screen[p].useDefFG)
	screen[p].fg = at->fg;
      else
	tmp.fg = screen[p].fg;

      if (screen[p].useDefBG)
	screen[p].bg = at->bg;
      else
	tmp.bg = screen[p].bg;

      if (screen[p].useDefAttr)
	screen[p].attr = at->curAttr;
      else
	tmp.curAttr = screen[p].attr;
    }

    /* Else use the current Cell's settings */
    else {
      tmp.curAttr = screen[p].attr;
      tmp.fg = screen[p].fg;
      tmp.bg = screen[p].bg;
    }

    eterm_echo(&tmp, p, screen[p].val, false);
  }

  /* Request one update of the screen region */
  eterm_update(at, 0, maxp);

  ShowCursor(at);
}

void
ansi_clear_screen(AnsiTerm *at)
{
  uint32_t u;

  if (at == NULL)
    return;

  for (u = 0; u < MAXPOS; u++) {
    screen[u].val = ' ';
    screen[u].fg = at->fg;
    screen[u].bg = at->bg;
    screen[u].attr = at->curAttr;
    screen[u].useDefFG = true;
    screen[u].useDefBG = true;
    screen[u].useDefAttr = true;
  }

  at->maxpos = at->pos = 0;
}

void
ansi_set_cursor_blink(AnsiTerm *at, bool on)
{
  if (at == NULL)
    return;

  at->blinkCursor = on;
}

void
ansi_resize(AnsiTerm *at, uint32_t new_rows, uint32_t new_cols)
{
  if (at == NULL)
    return;

  ansi_retype_all(at, EA_NORMAL);

  if(at->pos / at->cols > (new_rows-1)) {
    if (at->scroll) {
      uint32_t excess_rows = (at->pos/at->cols) - (new_rows-1);

      Scroll(at, 0, at->pos, - (int)(at->cols * excess_rows));
      at->pos -= (at->cols * excess_rows);
    }
    else {
      at->pos = (at->pos % at->cols); /* ??? */
    }
  }

  at->cols = new_cols;
  at->rows = new_rows;
}
