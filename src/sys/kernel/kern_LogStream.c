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

#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>

#define DIAG_BUF_PAGES	1u

static char diagbuf[DIAG_BUF_PAGES * EROS_PAGE_SIZE];

static char *const logbuf = diagbuf;
static char *nextin = diagbuf;
#if 0
/* Eventually to be used for user-level extraction of log output. */
static char *nextout = diagbuf;
#endif
static char *const logtop = diagbuf + (DIAG_BUF_PAGES * EROS_PAGE_SIZE);

extern void halt(char c) NORETURN;

void
LogStream_Put(uint8_t c)
{
  if ((unsigned)(nextin - logbuf) > (DIAG_BUF_PAGES * EROS_PAGE_SIZE))
    halt('p');
  
  if (nextin == logtop)
    nextin = logbuf;
  
  *nextin++ = c;
}
