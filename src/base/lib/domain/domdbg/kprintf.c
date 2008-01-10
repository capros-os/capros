/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
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

static const char small_digits[] = "0123456789abcdef";
static const char large_digits[] = "0123456789ABCDEF";

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

enum size_modifier {
  sizemod_none = 0,
  sizemod_l,
  sizemod_ll
};

static void
printf_guts(void (putc)(char c, buf *buffer),
	    buf *pBuf, const char *fmt, va_list ap)
{
  /* A long long has up to 22 octal digits. */
  char buf[25];
  uint32_t len;
  unsigned long ul;
  unsigned long long ull;
    
  if (fmt == 0) {		/* bogus input specifier */
    putc('?', pBuf);
    putc('f', pBuf);
    putc('m', pBuf);
    putc('t', pBuf);
    putc('?', pBuf);
    return;
  }

  for( ; *fmt; fmt++) {
    if (*fmt != '%') {
      putc(*fmt, pBuf);
      continue;
    }

    uint32_t width = 0;
    char sign = 0;
    int rightAdjust = TRUE;
    char fillchar = ' ';
    enum size_modifier sizemod = sizemod_none;
    const char * digits = small_digits;
    char * pend = &buf[25];
    char * p = pend;
    
    /* Process a conversion specification. */
    /* First the flags: */
  flags:
    fmt++;	// consume the '%' or flag

    switch (*fmt) {
      case '-': rightAdjust = FALSE; goto flags;
      case '0': fillchar = '0'; goto flags;
    default: break;
    }
     
    /* See if what follows is a width specifier: */

    while (*fmt >= '0' && *fmt <= '9') {
      width *= 10;
      width += (*fmt - '0');
      fmt++;
    }

    // See if there is a size modifier.

    switch (*fmt) {
    case 'l':
      fmt++;
      if (*fmt == 'l') {
        sizemod = sizemod_ll;
        break;
      } else {
        sizemod = sizemod_l;
        goto nosize;
      }
    default: goto nosize;
    }
    fmt++;
    nosize:
    
    switch (*fmt) {
    default:	// this case includes *fmt == 0
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
      switch (sizemod) {
      case sizemod_none:
	l = va_arg(ap, int);
        goto printl10;

      case sizemod_l:
	l = va_arg(ap, long);
printl10:
	if (l < 0) {
	  sign = '-';
          ul = -l;
        } else
          ul = l;
	goto printul10;

      case sizemod_ll:
        {
        long long ll = va_arg(ap, long long);
	if (ll < 0) {
	  sign = '-';
          ull = -ll;
        } else
          ull = ll;
	goto printull10;
        }
      break;
      }
    }

    case 'u':
      switch (sizemod) {
      case sizemod_none:
	ul = va_arg(ap, unsigned int);
        goto printul10;

      case sizemod_l:
	ul = va_arg(ap, unsigned long);
printul10:
        do {
	  *(--p) = digits[ul % 10];
	  ul /= 10;
        } while (ul);
	break;

      case sizemod_ll:
	ull = va_arg(ap, unsigned long long);
printull10:
        do {
	  *(--p) = digits[ull % 10];
	  ull /= 10;
        } while (ull);
	break;
      }
      break;

    case 't':
      {		/* for 2-digit time values */
	long l;

	l = va_arg(ap, long);
	      
	*(--p) = (l / 10) + '0';
	*(--p) = (l % 10) + '0';
	break;
      }
    case 'X':
      digits = large_digits;
    case 'x':
      switch (sizemod) {
      case sizemod_none:
	ul = va_arg(ap, unsigned int);
        goto printul16;

      case sizemod_l:
	ul = va_arg(ap, unsigned long);
printul16:
        do {
	  *(--p) = digits[ul & 0xf];
	  ul /= 16;
        } while (ul);
	break;

      case sizemod_ll:
	ull = va_arg(ap, unsigned long long);
        do {
	  *(--p) = digits[ull & 0xf];
	  ull /= 16;
        } while (ull);
	break;
      }
      break;

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

    if (sign)
      *(--p) = sign;

    len = pend - p;

    /* do padding with initial spaces for right justification: */
    if (width && rightAdjust && len < width) {
      while (len < width) {
	putc(fillchar, pBuf);
	width--;
      }
    }

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

  capros_Console_KDB(kr);
}

result_t
capros_Console_KDB(cap_t kr)
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
  return CALL(&msg);
}
