/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

/* kprintf -- printf via key.  Similar to fprintf, but not as complete
   and takes output stream key as first argument rather than FILE*.
   Implementation was stolen from the kernel code.
   */

#include <stddef.h>
#include <limits.h>
#include <eros/target.h>
#include <eros/stdarg.h>
#include <eros/Invoke.h>
#include <eros/ConsoleKey.h>
#include <domdbg/domdbg.h>

#define TRUE 1
#define FALSE 0

static const char hexdigits[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

#define hexdigit(b) (hexdigits[(b)])

#define bufsz 128
typedef struct {
  uint32_t streamKey;
  int len;
  char *txt;
} buf;

static void
flush(buf *pBuf)
{
  if (pBuf->len) {
    wrstream(pBuf->streamKey, pBuf->txt, pBuf->len);
    pBuf->len = 0;
  }
}

/* This should really be putc().  It is a temporary measure until
   we get line disciplines going. */
static void
do_putc(char c, buf *pBuf)
{
  if (pBuf->streamKey != KR_VOID && pBuf->len == bufsz)
    flush(pBuf);

  pBuf->txt[pBuf->len] = c;
  pBuf->len++;
}

static void
sprintf_putc(char c, buf *buffer)
{
  buffer->txt[buffer->len++] = c;
}

static void
printf_guts(void (putc)(char c, buf *buffer),
	    buf *pBuf, const char *fmt, void *vap)
{
  char sign = 0;
  uint32_t width = 0;
  int rightAdjust = TRUE;
  char fillchar = ' ';
  /* largest thing we might convert fits in 16 digits: */
  char buf[20];
  char *pend = &buf[20];
  char *p = pend;
  uint32_t len;
    
  va_list ap = (va_list) vap;

  if (fmt == 0) {		/* bogus input specifier */
    putc('?', pBuf);
    putc('f', pBuf);
    putc('m', pBuf);
    putc('t', pBuf);
    putc('?', pBuf);

    return;
  }

  for( ; *fmt; fmt++) {
    width = 0;
    sign = 0;
    rightAdjust = TRUE;
    fillchar = ' ';
    pend = &buf[20];
    p = pend;
    
    if (*fmt != '%') {
      putc(*fmt, pBuf);
      continue;
    }

    fmt++;

    /* check for left adjust.. */
    if (*fmt == '-') {
      rightAdjust = FALSE;
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
      putc('%', pBuf);
      putc('N', pBuf);
      putc('?', pBuf);
      return;
    }
        
    switch (*fmt) {
    default:
      {
	/* If we cannot interpret, we cannot go on */
	putc('%', pBuf);
	putc('?', pBuf);
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
	putc(fillchar, pBuf);
	width--;
      }
    }

    if (sign)
      putc('-', pBuf);
    
    /* output the text */
    while (p != pend)
      putc(*p++, pBuf);
    
    /* do padding with initial spaces for left justification: */
    if (width && rightAdjust == FALSE && len < width) {
      while (len < width) {
	putc(fillchar, pBuf);
	width--;
      }
    }
  }
}

int
sprintf(char *pBuf, const char* fmt, ...)
{
  buf theBuffer;

  va_list	listp;
  va_start(listp, fmt);

  /* We don't use the fixed text component. */
  theBuffer.txt = pBuf;
  theBuffer.len = 0;

  printf_guts(sprintf_putc, &theBuffer, fmt, listp);

  va_end(listp);

  return 0;
}

void
kvprintf(uint32_t kr, const char* fmt, va_list listp)
{
  char kprintf_str[bufsz];
  buf kprintf_buf;

  kprintf_buf.len = 0;
  kprintf_buf.streamKey = kr;
  kprintf_buf.txt = kprintf_str;

  printf_guts(do_putc, &kprintf_buf, fmt, listp);

  flush(&kprintf_buf);
}

void
kprintf(uint32_t kr, const char* fmt, ...)
{
  char kprintf_str[bufsz];
  buf kprintf_buf;

  va_list	listp;
  va_start(listp, fmt);

  kprintf_buf.len = 0;
  kprintf_buf.streamKey = kr;
  kprintf_buf.txt = kprintf_str;

  printf_guts(do_putc, &kprintf_buf, fmt, listp);

  flush(&kprintf_buf);

  va_end(listp);
}

void
kdprintf(uint32_t kr, const char* fmt, ...)
{
  char kprintf_str[bufsz];
  buf kprintf_buf;

  va_list	listp;
  va_start(listp, fmt);

  kprintf_buf.len = 0;
  kprintf_buf.streamKey = kr;
  kprintf_buf.txt = kprintf_str;

  printf_guts(do_putc, &kprintf_buf, fmt, listp);

  flush(&kprintf_buf);

  va_end(listp);

  {
    Message msg;

    msg.snd_key0 = KR_VOID;
    msg.snd_key1 = KR_VOID;
    msg.snd_key2 = KR_VOID;
    msg.snd_rsmkey = KR_VOID;

    msg.rcv_key0 = KR_VOID;
    msg.rcv_key1 = KR_VOID;
    msg.rcv_key2 = KR_VOID;
    msg.rcv_rsmkey = KR_VOID;

    msg.snd_len = 0;		/* no data sent */
    msg.rcv_limit = 0;		/* no data returned */

    msg.snd_code = OC_Console_KDB;
    msg.snd_invKey = kr;
    (void) CALL(&msg);
  }
}

