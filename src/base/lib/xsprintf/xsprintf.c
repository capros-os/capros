/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>
#include <eros/stdarg.h>

#include <string.h>
#include <limits.h>

#include "xsprintf.h"

#define bufsz 128
typedef struct {
  uint32_t streamKey;
  int32_t len;
  char * txt;
} buf;

static const uint8_t hexdigits[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

#define hexdigit(b) (hexdigits[(b)])

static void
do_putc(uint8_t c, buf *buffer)
{
  buffer->txt[buffer->len++] = c;
}

static void
doprintf(buf * pBuf, const char * fmt, void * vap)
{
  uint8_t sign = 0;
  uint32_t width = 0;
  bool rightAdjust = true;
  uint8_t fillchar = ' ';
  /* largest thing we might convert fits in 16 digits: */
  char buf[20];
  char * pend = &buf[20];
  char * p = pend;
  uint32_t len;
    
  va_list ap = (va_list) vap;

  if (fmt == 0) {		/* bogus input specifier */
    do_putc('?', pBuf);
    do_putc('f', pBuf);
    do_putc('m', pBuf);
    do_putc('t', pBuf);
    do_putc('?', pBuf);

    return;
  }

  for( ; *fmt; fmt++) {
    width = 0;
    sign = 0;
    rightAdjust = true;
    fillchar = ' ';
    pend = &buf[20];
    p = pend;
    
    if (*fmt != '%') {
      do_putc(*fmt, pBuf);
      continue;
    }

    fmt++;

    /* check for left adjust.. */
    if (*fmt == '-') {
      rightAdjust = false;
      fmt++;
    }
      
    /* we just saw a format character.  See if what follows is a width
       specifier: */

    if (*fmt == '0')
      fillchar = '0';

    while (*fmt && *fmt >= '0' && *fmt <= '9') {
      width *= 10;
      width += (*fmt - '0');
      fmt++;
    }
    
    if (*fmt == 0) {		/* bogus input specifier */
      do_putc('%', pBuf);
      do_putc('N', pBuf);
      do_putc('?', pBuf);
      return;
    }
        
    switch (*fmt) {
    default:
      {
	/* If we cannot interpret, we cannot go on */
	do_putc('%', pBuf);
	do_putc('?', pBuf);
	return;
      }
    case 'c':
      {
	char c;
	c = va_arg(ap, char);
	*(--p) = c;
	break;
      }	    
    case 'd':
    case 'i': /* JONADAMS: handle %i as %d */
      {
	long l;
	unsigned long ul;

	l = va_arg(ap, long);
	      
	if (l == 0) {
	  *(--p) = '0';
	}
	else {
	  if (l < 0)
	    sign = '-';

	  ul = (l < 0) ? (unsigned) -l : (unsigned) l;

	  if (l == LONG_MIN)
	    ul = ((unsigned long) LONG_MAX) + 1ul;

	  while(ul) {
	    *(--p) = '0' + (ul % 10);
	    ul = ul / 10;
	  }
	}
	break;
      }
    case 'u':
      {
	unsigned long ul;

	ul = va_arg(ap, unsigned long);
	      
	if (ul == 0) {
	  *(--p) = '0';
	}
	else {
	  while(ul) {
	    *(--p) = '0' + (ul % 10);
	    ul = ul / 10;
	  }
	}
	break;
      }
    case 'U':
      {
	unsigned long long ull;

	ull = va_arg(ap, unsigned long long);
	      
	if (ull == 0) {
	  *(--p) = '0';
	}
	else {
	  while(ull) {
	    *(--p) = '0' + (ull % 10u);
	    ull = ull / 10u;
	  }
	}
	break;
      }
    case 't':
      {		/* for 2-digit time values */
	long l;

	l = va_arg(ap, long);
	      
	*(--p) = (l / 10) + '0';
	*(--p) = (l % 10) + '0';
	break;
      }
    case 'x':
      {
	unsigned long ul;
	    
	ul = va_arg(ap, unsigned long);
	      
	if (ul == 0) {
	  *(--p) = '0';
	}
	else {
	  while(ul) {
	    *(--p) = hexdigit(ul & 0xfu);
	    ul = ul / 16;
	  }
	}

	break;
      }
    case 'X':
      {
	unsigned long long ull;
	    
	ull = va_arg(ap, unsigned long long);
	      
	if (ull == 0) {
	  *(--p) = '0';
	}
	else {
	  while(ull) {
	    *(--p) = hexdigit(ull & 0xfu);
	    ull = ull / 16u;
	  }
	}

	break;
      }
    case 's':
      {
	p = pend = va_arg(ap, char *);
	      
	while (*pend)
	  pend++;
	break;
      }
    case '%':
      {
	*(--p) = '%';
	break;
      }
    }

    len = pend - p;
    if (sign)
      len++;

    /* do padding with initial spaces for right justification: */
    if (width && rightAdjust && len < width) {
      while (len < width) {
	do_putc(fillchar, pBuf);
	width--;
      }
    }

    if (sign)
      do_putc('-', pBuf);
    
    /* output the text */
    while (p != pend)
      do_putc(*p++, pBuf);
    
    /* do padding with initial spaces for left justification: */
    if (width && rightAdjust == false && len < width) {
      while (len < width) {
	do_putc(fillchar, pBuf);
	width--;
      }
    }
  }
}

uint32_t 
xsprintf(char * str, const char * format, ...)
{
  va_list args;
  buf msg_buf;
  char msg_str[bufsz];
  uint32_t u;
  uint32_t len = 0;

  for (u = 0; u < bufsz; u++)
    msg_str[u] = '\0';

  va_start(args, format);
  msg_buf.len = 0;
  msg_buf.streamKey = 0;
  msg_buf.txt = msg_str;

  doprintf(&msg_buf, format, args);
  va_end(args);

  len = strlen(msg_str);
  strncpy(str, msg_str, len);
  str[len] = '\0';

  return len;
}
