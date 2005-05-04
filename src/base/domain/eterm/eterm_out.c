
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

/* A domain that manages the contents of a window on behalf of a
   window system client.  The contents are treated as a "text
   console". */
#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>
#include <eros/cap-instr.h>

#include <string.h>
#include <ctype.h>

#include <domain/SpaceBankKey.h>
#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Include the needed interfaces */
#include <domain/SessionKey.h>
#include <idl/eros/Stream.h>
#include <idl/eros/domain/timer/manager.h>
#include "idl/eros/domain/eterm.h"

#include <graphics/color.h>
#include <graphics/rect.h>
#include <graphics/fonts/Font.h>

#include <addrspace/addrspace.h>

#include "constituents.h"

/* ANSI terminal emulation */
#include "eterm_ansi.h"

/* The largest size any domain currently has for sharing or otherwise
mapping memory is 1GB.  This domain maps an array of 32-bit values
into that region and the offset into this region starts at zero. */
#define _1GB_ 0x40000000u
#define MAX_BUFFER_OFFSET ((_1GB_ / sizeof(uint32_t)) - 1)

#define KR_OSTREAM  KR_APP(0) /* For debugging output via kprintf*/
#define KR_SCRATCH  KR_APP(1)
#define KR_START    KR_APP(2)
#define KR_NODE     KR_APP(3)
#define KR_STASH    KR_APP(4)
#define KR_SESSION  KR_APP(5)
#define KR_SEGMENT  KR_APP(6)
#define KR_START_STREAM  KR_APP(7)
#define KR_ETERM_IN KR_APP(8)
#define KR_TIMER    KR_APP(9)
#define KR_START_FOR_TIMER  KR_APP(10)
#define KR_PASTE_CONTENT    KR_APP(11)
#define KR_PASTE_CONVERTER  KR_APP(12)

/* Interfaces this guy implements */
#define ETERM_MAIN_INTERFACE  0x01
#define ETERM_STREAM_INTERFACE 0x02
#define ETERM_TIMER_INTERFACE  0x03

/* globals */
AnsiTerm at;
FontType *curFont = NULL;
uint32_t main_width = 0;
uint32_t main_height = 0;
uint32_t main_window_id = 0;
uint32_t *main_content = NULL;
point_t TOPLEFT = {3, 3};
static bool blink_toggle = false;
int32_t cursorBusyCount = 0;

uint32_t receive_buffer[1024];

cap_t kr_ostream = KR_OSTREAM;

// #define TRACE

#define DEFAULT_ROWS 24
#define DEFAULT_COLS 80
#define DEFAULT_CURSOR 219

#define DEBUG_ETERM_OUT if(0)

/* Ask window system to redraw our window contents */
static void
update(rect_t area)
{
  (void) session_win_redraw(KR_SESSION, main_window_id,
			    area.topLeft.x, area.topLeft.y,
			    area.bottomRight.x, area.bottomRight.y);
}

/* Clear a specified rectangular region of our window to the specified
   color */
static void
doClear(rect_t area, color_t color)
{
  uint32_t x, y;

  for (x = area.topLeft.x; x < area.bottomRight.x; x++)
    for (y = area.topLeft.y; y < area.bottomRight.y; y++)
       main_content[x + y * main_width] = color;
}

/* Clear the entire window to a predetermined color */
static void
clear(void)
{
  rect_t area = { {0,0}, {main_width, main_height} };

  doClear(area, at.bg);
  update(area);
}

void
eterm_update(AnsiTerm *at, uint32_t startPos, uint32_t endPos)
{
  uint32_t posRow = 0;
  uint32_t posCol = 0;
  point_t pixel;
  rect_t area;

  if (at == NULL)
    return;

  /* Compute upper left corner of update region by taking the
     left-most position in the row of the startPos. */
  posRow = startPos / at->cols;
  posCol = 0;
  pixel.x = posCol * curFont->width;
  pixel.y = posRow * curFont->height;
  area.topLeft = pixel;

  /* Compute bottom right corner of update region by taking the
     right-most position in the row of the endPos. */
  posRow = endPos / at->cols + 1;
  posCol = at->cols;
  pixel.x = posCol * curFont->width;
  pixel.y = posRow * curFont->height;
  area.bottomRight = pixel;

  update(area);
}

static void
doEcho(AnsiTerm *at, uint32_t pos, uint16_t c, bool withUpdate, uint16_t flag)
{
  uint8_t str[2] = {(uint8_t)c, '\0'};
  point_t pixel;
  rect_t area;
  uint32_t colno, rowno;
  color_t fg;
  color_t bg;

  if (at == NULL)
    return;

  if (at->curAttr == Invisible)
    return;

  if (at->curAttr == Reverse) {
    fg = at->bg;
    bg = at->fg;
  }
  else {
    fg = at->fg;
    bg = at->bg;
  }

  colno = pos % at->cols;
  rowno = pos / at->cols;

  pixel.x = colno * curFont->width;
  pixel.y = rowno * curFont->height;
  area.topLeft = pixel;
  area.bottomRight.x = pixel.x + curFont->width;
  area.bottomRight.y = pixel.y + curFont->height;

  font_render(curFont,(uint8_t *)main_content, main_width,
	      pixel, fg, bg, flag, str);

  if (withUpdate)
    update(area);

}

void
eterm_echo(AnsiTerm *at, uint32_t pos, uint16_t c, bool withUpdate)
{
  if (at == NULL)
    return;

  if (at->curAttr == Underline)
    doEcho(at, pos, c, withUpdate, FONT_UNDERLINE);
  else
    doEcho(at, pos, c, withUpdate, FONT_NORMAL);
}

void
eterm_echo_cursor(AnsiTerm *at, uint32_t pos, bool withUpdate)
{
  if (at == NULL)
    return;

  doEcho(at, pos, at->cursor, withUpdate, FONT_XOR);
}

void
eterm_set_cursor_blink(bool toggle)
{
  ansi_set_cursor_blink(&at, toggle);
}

static bool
Timer_Request(Message *m)
{
  /* Ignore timer during lots of output */
  if (cursorBusyCount > 0)
    cursorBusyCount--;
  else {
    blink_toggle = (blink_toggle == true) ? false : true;
    ansi_make_blink(&at, blink_toggle);
  }
  m->snd_code = RC_OK;
  return true;
}

static void
doPutChar(uint32_t c)
{
  if (isalnum(c))
    { DEBUG_ETERM_OUT kprintf(KR_OSTREAM, "Char is '%c'\n", c);}
  else
    {
      DEBUG_ETERM_OUT kprintf(KR_OSTREAM, "Char is '\\x%x'\n", c);
    }

  /* Set number of timer signals to ignore before blinking again */
  cursorBusyCount = 1;	

  if (!blink_toggle) {
    blink_toggle = true;
    ansi_make_blink(&at, blink_toggle);
  }
  ansi_put(&at, (uint8_t)c);
}

static bool
Stream_Request(Message *m)
{
  switch(m->rcv_code) {

  case OC_eros_Stream_read:
    {
      /* Send the caller to the eterm_main domain */
      m->snd_key0 = KR_ETERM_IN;
      m->invType = IT_Retry;
      m->snd_w1 = RETRY_SET_LIK;
      m->snd_w2 = 0;
    }
    break;

  case OC_eros_Stream_write:
    {
      doPutChar(m->rcv_w1);
      m->snd_code = RC_OK;
    }
    break;
    
  case OC_eros_Stream_nwrite:
    {
      eros_Stream_iobuf *s = (eros_Stream_iobuf *)(m->rcv_data);
      uint32_t u;

      s->data = (char *)(m->rcv_data + 16);
  
      kprintf(KR_OSTREAM, "ETERM_out: nwrite w/string [%s]\n", s->data);
      for (u = 0; u < s->len; u++)
	doPutChar(s->data[u]);

      m->snd_code = RC_OK;
    }
    break;

  default:
    break;
  }
  return true;
}

static bool
ETerm_Request(Message *m)
{
  switch(m->rcv_code) {
  case OC_eros_domain_eterm_stream_open:
    {
      m->snd_key0 = KR_START_STREAM;
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_initialize:
    {
      uint32_t u;
      result_t result;

      COPY_KEYREG(KR_RETURN, KR_STASH);
      COPY_KEYREG(KR_ARG(0), KR_SESSION);

      /* Create a window via the session key */
      result = session_new_default_window(KR_SESSION,
					  KR_BANK,
					  DEFAULT_PARENT,
					  &main_window_id,
					  KR_SEGMENT);

      if (result != RC_OK) {
	kprintf(KR_OSTREAM, "Error: EtermOut: couldn't create new window: "
		"0x%08x (%d) (%u)\n",
		result, result, result);
	m->snd_code = RC_eros_key_RequestError;
	COPY_KEYREG(KR_STASH, KR_RETURN);
	return true;
      }

      /* Compute window size for 80x24 */
      curFont = font_find_font("Default");
      if (curFont == NULL) {
	kprintf(KR_OSTREAM, "** FATAL Error: EtermOut: couldn't get default "
		"font! Terminating...\n");
	m->snd_code = RC_eros_key_RequestError;
	return false;
      }

      main_width = DEFAULT_COLS*curFont->width;
      main_height = DEFAULT_ROWS*curFont->height;

      /* Resize the window */
      session_win_resize(KR_SESSION, main_window_id, main_width, main_height);

      /* Change the default window title to something meaningful */	  
      session_win_set_title(KR_SESSION, main_window_id, 
			    "ETerm");

      kprintf(KR_OSTREAM, "EtermOut: width=%u  height=%u\n",
	      main_width, main_height);

      /* Map the new addr space into this domain's space */
      process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
      node_swap(KR_SCRATCH, 16, KR_SEGMENT, KR_VOID);

      /* In order to access this mapped space, this domain needs a
	 well-known address:  use the well-known address that corresponds
	 to slot 16 :*/
      main_content = (uint32_t *)0x80000000u;

      /* In order to make full use of the entire 1GB space (for resizing
	 or doing other "offscreen memory" tasks), we need
	 local window keys in the rest of the available slots */
      for (u = 17; u < 24; u++)
	addrspace_insert_lwk(KR_SCRATCH, 16, u, EROS_ADDRESS_LSS);

      clear();
      session_win_map(KR_SESSION, main_window_id);

      eros_domain_timer_manager_set_interval(KR_TIMER, 1000);
      eros_domain_timer_manager_start_timer(KR_TIMER);

      kprintf(KR_OSTREAM, "EtermOut: initialize done.\n");

      COPY_KEYREG(KR_STASH, KR_RETURN);

      /* Return the window id */
      m->snd_w1 = main_window_id;
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_set_title:
    {
      eros_domain_eterm_titlestr *title =
	(eros_domain_eterm_titlestr *)(m->rcv_data + 0);

      title->data = (char *) (m->rcv_data + 16);

      COPY_KEYREG(KR_RETURN, KR_STASH);
      session_win_set_title(KR_SESSION, main_window_id, title->data);
      COPY_KEYREG(KR_STASH, KR_RETURN);

      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_set_cursor:
    {
      ansi_set_cursor(&at, (uint16_t)m->rcv_w1);
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_get_fonts:
    {
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_set_font:
    {
      uint32_t font_index = m->rcv_w1;

      /* Fix: Need a better way to select a font, but CapIDL doesn't
      seem to quite support retrieving a list of strings from the
      server. */
      COPY_KEYREG(KR_RETURN, KR_STASH);

      if (font_index < font_num_fonts()) {
	if (all_fonts[font_index] && 
	    (uint32_t)curFont != (uint32_t)(&all_fonts[font_index])) {

	  curFont = all_fonts[font_index];
	  main_width = DEFAULT_COLS*curFont->width;
	  main_height = DEFAULT_ROWS*curFont->height;

	  /* Ask window system to resize the window to accomodate the
	     new font size */
	  session_win_resize(KR_SESSION, main_window_id, main_width, 
			     main_height);

	  /* Clear the newly resized window */
	  clear();

	  /* Retype the contents using the new font */
	  ansi_retype_all(&at, EA_NORMAL);
	}
      }
      COPY_KEYREG(KR_STASH, KR_RETURN);
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_set_bg_color:
    {
      at.bg = m->rcv_w1;
      clear();
      ansi_retype_all(&at, EA_WITH_NEW_ATTRS);
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_set_fg_color:
    {
      at.fg = m->rcv_w1;
      clear();
      ansi_retype_all(&at, EA_WITH_NEW_ATTRS);
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_sepuku:
    {
      /* FIX: This doesn't really commit sepuku, it just kills the
      window.  This needs to be rewritten to really commit sepuku but
      return to the caller before going completely away. */
      kprintf(KR_OSTREAM, "ETERM: committing sepuku...\n");
      COPY_KEYREG(KR_RETURN, KR_STASH);
      session_win_kill(KR_SESSION, main_window_id);
      eros_domain_timer_manager_stop_timer(KR_TIMER);
      COPY_KEYREG(KR_STASH, KR_RETURN);
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_clear:
    {
      COPY_KEYREG(KR_RETURN, KR_STASH);
      clear();
      ansi_clear_screen(&at);
      COPY_KEYREG(KR_STASH, KR_RETURN);
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_scroll:
    {
      kprintf(KR_OSTREAM, "** Scrolling not implemented yet.\n");
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_resize:
    {
      COPY_KEYREG(KR_RETURN, KR_STASH);

      /* Modify the current window dimensions */
      main_width = m->rcv_w1;
      main_height = m->rcv_w2;

      /* On a resize, we don't change the number of columns, but we do
         reset the number of rows.  This ensures that the most recent
         line is always displayed. */
      {
	uint32_t new_rows = main_height / curFont->height;

	/* Only allow new height to be multiples of font height */
	main_height = new_rows * curFont->height;
	if (main_height != m->rcv_w2)
	  session_win_resize(KR_SESSION, main_window_id, main_width, 
			     main_height);

	clear();
	ansi_resize(&at, new_rows, at.cols);
      }

      COPY_KEYREG(KR_STASH, KR_RETURN);

      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_highlight:
    {
      kprintf(KR_OSTREAM, "** Highlighting not implemented yet.\n");
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_set_cursor_blink:
    {
      eterm_set_cursor_blink((bool)m->rcv_w1);
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_eterm_put_pastebuffer:
    {
      m->snd_code = session_put_pastebuffer(KR_SESSION, KR_ARG(0), KR_ARG(1));
    }
    break;

  case OC_eros_domain_eterm_get_pastebuffer:
    {
      m->snd_code = session_get_pastebuffer(KR_SESSION, KR_PASTE_CONTENT, 
					    KR_PASTE_CONVERTER);
      if (m->snd_code == RC_OK) {
	m->snd_key0 = KR_PASTE_CONTENT;
	m->snd_key1 = KR_PASTE_CONVERTER;
      }
    }
    break;

  default:
    break;
  }
  return true;
}

static bool
ProcessRequest(Message *m)
{
  switch(m->rcv_keyInfo) {
  case ETERM_TIMER_INTERFACE:
    {
      return Timer_Request(m);
    }
    break;

  case ETERM_MAIN_INTERFACE:
    {
      return ETerm_Request(m);
    }
    break;

  case ETERM_STREAM_INTERFACE:
    {
      return Stream_Request(m);
    }
    break;

  default:
    {
      m->snd_code = RC_eros_key_UnknownRequest;
    }
  }
  return true;
}

static void
doClearRcv(void)
{
  uint32_t u;

  for (u = 0; u < 1024; u++)
    receive_buffer[u] = 0;
}

int
main(void)
{
  Message msg;

  /* Stash start key to eterm_main */
  COPY_KEYREG(KR_ARG(0), KR_ETERM_IN);

  /* Copy keys from the constituent's node to the keyreg set. */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_TIMER, KR_TIMER);

  kprintf(KR_OSTREAM, "Eterm_Out says hello...\n");

  /* Fabricate start keys */
  process_make_start_key(KR_SELF, ETERM_MAIN_INTERFACE, KR_START);
  process_make_start_key(KR_SELF, ETERM_STREAM_INTERFACE, KR_START_STREAM);
  process_make_start_key(KR_SELF, ETERM_TIMER_INTERFACE, KR_START_FOR_TIMER);

  /* Construct a timer domain for blinking characters */
  kprintf(KR_OSTREAM, "Eterm_Out constructing timer domain...\n");
  if (constructor_request(KR_TIMER, KR_BANK, KR_SCHED, KR_START_FOR_TIMER, 
			  KR_TIMER) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: Eterm Out couldn't construct timer!\n");
    return -1;
  }

  /* Prepare our default address space for mapping in additional
     memory. */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, KR_NODE);

  /* Initialize ANSI terminal emulation logic */
  ansi_reset(&at, DEFAULT_ROWS, DEFAULT_COLS);
  ansi_set_cursor(&at, DEFAULT_CURSOR);

  memset(&msg, 0, sizeof(Message));
  msg.rcv_rsmkey = KR_RETURN;
  msg.snd_key0 = KR_START;	/* return first start key to ctor */
  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_data = &receive_buffer;
  msg.rcv_limit = sizeof(receive_buffer);

  msg.invType = IT_PReturn;
  do {
    msg.snd_invKey = KR_RETURN;
    msg.rcv_rsmkey = KR_RETURN;
    doClearRcv();
    INVOKECAP(&msg);
    msg.snd_key0 = KR_VOID;
    msg.snd_w1 = 0;
    msg.invType = IT_PReturn;
  } while (ProcessRequest(&msg));

  return 0;
}

