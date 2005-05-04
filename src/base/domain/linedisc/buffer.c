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

/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stddef.h>
#include <ctype.h>

#include <eros/target.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <idl/eros/Stream.h>

#include "keydefs.h"
#include "buffer.h"

/* FIX: Don't we have one approved, public definition for min and max? */
#ifndef min
  #define min(a,b) ((a) <= (b) ? (a) : (b))
#endif

typedef enum bstate bstate;
enum bstate {
  Normal,
  SawEscape,
  WantParam,
};

#define MAXPARAM 2
static bstate state = Normal;
static uint32_t param[MAXPARAM];
static uint32_t nparams = 0;

#define FORWARD  0x00u
#define BACKWARD 0x01u
static void
move_cursor(LDBuffer *buf, cap_t strm, uint8_t how, uint32_t count)
{
  uint32_t cnt = count;
  uint16_t c;

  if (how != FORWARD && how != BACKWARD)
    kdprintf(KR_OSTREAM, "Bad argument to move_cursor: 0x%02x\n", how);

  c = (how == BACKWARD) ? 0x44 : 0x43;

  while(cnt) {

    /* Send escape sequence downstream */
    eros_Stream_write(strm, 0x1b);
    eros_Stream_write(strm, 0x5b);
    eros_Stream_write(strm, '1');
    eros_Stream_write(strm, c);

    if (how == FORWARD) {
      buf->cursor++;
      if (buf->cursor > buf->len)
	break;
    } else if (how == BACKWARD) {
      buf->cursor--;
      if (buf->cursor == 0)
	break;
    }

    cnt--;
  }
}

static void
processEsc(cap_t strm, LDBuffer *buf, uint16_t c)
{
  if (buf == NULL)
    return;

  /* We only need to worry about escape sequences that move the
     cursor.  The cursor location is bounded by the input buffer. */
  switch (c) {
  case 0x44:			/* 'D' (move backward) */
    {
      /* Only move backward if cursor is not at beginning of line */
      if (buf->cursor > 0)
	move_cursor(buf, strm, BACKWARD, param[0]);
    }
    break;

  case 0x43:			/* 'C' (move forward) */
    {
      /* Only move forward if cursor is not already at end of current
	 line */
      if (buf->cursor < buf->len)
	move_cursor(buf, strm, FORWARD, param[0]);
    }
    break;

  default:		
    {
      state = Normal;
    }
  }
  state = Normal;
}

static void
buffer_insert_chars(LDBuffer *buf, uint32_t pos, uint32_t numChars)
{
  uint32_t tmpPos = min(buf->len + numChars - 1, MAXLEN - 1);

  while (tmpPos > pos) {
    buf->chars[tmpPos] = buf->chars[tmpPos-numChars];
    buf->chars[tmpPos-numChars] = 0;
    tmpPos--;
  }

  buf->len += numChars;
}

/* Remove characters to the left of the 'pos' and "slide" everything
   to the right over to compensate. */
static void
buffer_remove_chars(LDBuffer *buf, uint32_t pos, uint32_t numChars)
{
  uint32_t tmpPos = pos;

  while (tmpPos < buf->len) {
    buf->chars[tmpPos-numChars] = buf->chars[tmpPos];
    buf->chars[tmpPos] = 0;
    tmpPos++;
  }

  buf->len -= numChars;
}

void
buffer_dump(LDBuffer *buf)
{
  int32_t u;

  kprintf(KR_OSTREAM, "Buffer cursor: %u\n", buf->cursor);
  kprintf(KR_OSTREAM, "Buffer length: %u\n", buf->len);
  kprintf(KR_OSTREAM, "Buffer contents ==>\n");
  for (u = 0; u < buf->len; u++) {
    if (isalnum(buf->chars[u]))
      kprintf(KR_OSTREAM, "%c", buf->chars[u]);
    else
      kprintf(KR_OSTREAM, "?");
  }
  kprintf(KR_OSTREAM, "\n\n");
}

static void
doBufferWrite(cap_t strm, LDBuffer *buf, eros_domain_linedisc_termios tstate, 
	      uint16_t c)
{
  /* Need to check where virtual cursor is.  We might be inserting
     characters into the middle of the buffer. */
  if (buf->cursor < buf->len) {
    buffer_insert_chars(buf, buf->cursor, 1);

    /* Send escape sequence (for insert 1 char) downstream */
    eros_Stream_write(strm, 0x1b);
    eros_Stream_write(strm, 0x5b);
    eros_Stream_write(strm, '1');
    eros_Stream_write(strm, '@');
  }
  else
    buf->len++;

  /* Proceed with writing char */
  buf->chars[buf->cursor] = c;
  buf->cursor++;

  /* Echo character */
  if (ISSET(tstate.c_lflag, eros_domain_linedisc_ECHO))
    eros_Stream_write(strm, c);
}

static void
processChar(cap_t strm, LDBuffer *buf, eros_domain_linedisc_termios tstate, 
	    uint16_t c)
{
  if (buf == NULL)
    return;

  switch(state) {
  case Normal:
    {
      if (c == 0x1b)
	state = SawEscape;
      else {
	if (isprint(c))
	  doBufferWrite(strm, buf, tstate, c);
	//	buffer_dump(buf);
      }
    }
    break;

  case SawEscape:
    {
      int i;

      if (c == 0x5b) {
	state = WantParam;
	nparams = 0;
	for (i = 0; i < MAXPARAM; i++)
	  param[i] = 0;
      }
      else
	state = Normal;
    }
    break;

  case WantParam:
    {
      if (nparams < 2 && c >= '0' && c <= '9') {
	param[nparams] *= 10;
	param[nparams] += (c - '0');
      }
      else if (nparams < 2 && c == ';')
	nparams++;
      else if (nparams == 2)
	state = Normal;
      else {
	nparams++;
	processEsc(strm, buf, c);
      }
    }
    break;

  default:
    {
      state = Normal;
    }
    break;
  }
}

static void
buf_unputc(cap_t strm, LDBuffer *buf)
{
  int32_t pos = buf->len - 1;

  /* Need to check where virtual cursor is.  We might be deleting
  characters from the middle of the buffer, in which case we need to
  scroll to the left. */
  if (buf->cursor <= pos && buf->cursor > 0) {
    buffer_remove_chars(buf, buf->cursor, 1);

    move_cursor(buf, strm, BACKWARD, 1);

    /* Send escape sequence (for scroll left) downstream */
    eros_Stream_write(strm, 0x1b);
    eros_Stream_write(strm, 0x5b);
    eros_Stream_write(strm, '1');
    eros_Stream_write(strm, 'P');
  }
  else if (buf->cursor > 0 && pos >= 0 && pos < MAXLEN) {

    buf->chars[pos] = 0;
    buf->cursor = pos;
    buf->len--;

    /* FIX: We need a standard definition of canonical key codes that
       both eterm and ld0 can agree upon. */
    eros_Stream_write(strm, 0x08); /* BS */
  }
}

void
buffer_clear(LDBuffer *buf)
{
  uint32_t u;

  for (u = 0; u < MAXLEN; u++)
    buf->chars[u] = 0;

  buf->cursor = buf->len = 0; 
}

bool
buffer_write(cap_t strm, eros_domain_linedisc_termios tstate, 
	     LDBuffer *buf, uint16_t c)
{
#define M(nm) eros_domain_linedisc_##nm


  if (buf->len == MAXLEN)
    return false;

  /* Process the following line discipline special chars:
   *
   * VERASE   => deleting one character
   * VKILL    => delete entire buffer
   * VWERASE  => delete word
   * VREPRINT => retype buffer
   */

  /* Only process special chars if in canonical mode */
  if (ISSET(tstate.c_lflag, M(ICANON))) {

    if (CCEQ(tstate.c_cc[M(VERASE)], c)) {

      /* Check if there are any entries to erase */
      if (buf->len > 0) {
	buf_unputc(strm, buf);
      }
      return true;
    }
    else if (CCEQ(tstate.c_cc[M(VKILL)], c)) {

      if (ISSET(tstate.c_lflag, M(ECHOKE)) && 
	  !ISSET(tstate.c_lflag, M(ECHOPRT))) {

        /* Erase line via BS-SP-BS (FIX: This is the best way to do
	   this right now until stream_nwrite is working correctly!) */ 
        while (buf->cursor > 0)
	  buf_unputc(strm, buf);
      }
      else {
	eros_Stream_write(strm, c);
	if (ISSET(tstate.c_lflag, M(ECHOK)))
	  eros_Stream_write(strm, '\n');
	buffer_clear(buf);
      }
      return true;
    }
    else if (CCEQ(tstate.c_cc[M(VWERASE)], c)) {

      if (buf->cursor == buf->len) {
      /* First erase spaces */
      while ((buf->len > 0) && (buf->chars[buf->len-1] == ' ' ||
				buf->chars[buf->len-1] == '\t'))
	buf_unputc(strm, buf);

      /* Now erase word */
      while (buf->len > 0 && buf->chars[buf->len-1] != ' ' &&
	     buf->chars[buf->len-1] != '\t')
	buf_unputc(strm, buf);
      }
      else {
	while (buf->chars[buf->cursor] == ' ' ||
	       buf->chars[buf->cursor] == '\t')
	  buf_unputc(strm, buf); 

	while (buf->chars[buf->cursor] != ' ' &&
	       buf->chars[buf->cursor] != '\t')
	  buf_unputc(strm, buf); 
      }

      return true;
    }
    else if (CCEQ(tstate.c_cc[M(VREPRINT)], c)) {
      return true;
    }

  }

  /* All other characters:  Use a state machine to intercept ANSI
     escape sequences.  For example, don't issue escape sequences that
     will move cursor beyond start of input line. */
  processChar(strm, buf, tstate, c);

  return true;

#undef M
}
