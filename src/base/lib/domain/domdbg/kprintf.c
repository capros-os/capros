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
#include <string.h>
#include <limits.h>
#include <eros/target.h>
#include <eros/stdarg.h>
#include <eros/Invoke.h>
#include <eros/ConsoleKey.h>
#include <domdbg/domdbg.h>

static const char small_digits[] = "0123456789abcdefx";
static const char large_digits[] = "0123456789ABCDEFX";

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

void
printPrefixSign(void (putc)(char c, buf *buffer), buf * pBuf,
  char sign, bool sharpflag, int base, const char * digits)
{
  if (sign)	// add sign
    putc(sign, pBuf);
  else 
    // Add prefix.
    if (sharpflag) {
      if (base == 8) {
        putc('0', pBuf);
      } else if (base == 16) {
        putc('0', pBuf);
        putc(digits[16], pBuf);      // 'x' or 'X'
      }
    }
}

enum size_modifier {
  sizemod_none = 0,
  sizemod_h,
  sizemod_l,
  sizemod_ll
};

static void
printf_guts(void (putc)(char c, buf *buffer),
	    buf *pBuf, const char *fmt, va_list ap)
{
  /* A long long has up to 22 octal digits.
  Add 2 for prefix "0x" and 1 for sign. */
  char buf[25];
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

    int base = 0;
    int width = 0;
    char sign = 0;
    int leftAdjust = false;
    char padchar = ' ';
    bool sharpflag = false;
    int precision = -1;	// so far, not present
    enum size_modifier sizemod = sizemod_none;
    const char * digits = small_digits;
    char * pend = &buf[25];
    char * p = pend;
    
    /* Process a conversion specification. */
    /* First the flags: */
  flags:
    fmt++;	// consume the '%' or flag

    switch (*fmt) {
      case '-': leftAdjust = true; goto flags;
      case '0': padchar = '0'; goto flags;
      case '#': sharpflag = true; goto flags;
    default: break;
    }
     
    /* See if what follows is a width specifier: */

    if (*fmt == '*') {
      fmt++;
      width = va_arg (ap, int);
      if (width < 0) {
        leftAdjust = !leftAdjust;
        width = -width;
      }
    } else {
      while (*fmt >= '0' && *fmt <= '9') {
        width *= 10;
        width += (*fmt - '0');
        fmt++;
      }
    }
     
    /* See if what follows is a precision specifier: */

    if (*fmt == '.') {
      fmt++;
      precision = 0;
      if (*fmt == '*') {
        fmt++;
        precision = va_arg (ap, int);
        if (precision < 0) {
          precision = -precision;
        }
      } else {
        while (*fmt >= '0' && *fmt <= '9') {
          precision *= 10;
          precision += (*fmt - '0');
          fmt++;
        }
      }
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

    case 'h':
      sizemod = sizemod_h;
      break;

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

    case 't':
      {		/* for 2-digit time values */
	long l;

	l = va_arg(ap, long);
	      
	*(--p) = (l / 10) + '0';
	*(--p) = (l % 10) + '0';
	break;
      }

    case 'd':
    case 'i': /* handle %i as %d */
    {
      long l;
      base = 10;
      switch (sizemod) {
      case sizemod_none:
	l = va_arg(ap, int);
        break;

      case sizemod_h:
	l = (short) va_arg(ap, int);
        break;

      case sizemod_l:
	l = va_arg(ap, long);
        break;

      default:	// should not happen
      case sizemod_ll:
        {
        long long ll = va_arg(ap, long long);
	if (ll < 0) {
	  sign = '-';
          ull = -ll;
        } else
          ull = ll;
	goto printull;
        }
      }
      if (l < 0) {
        sign = '-';
        ul = -l;
      } else
        ul = l;
      goto printul;
    }

    case 'p':
      sharpflag = true;
      goto hex;

    case 'o':
      base = 8;
      goto unumbers;
    case 'u':
      base = 10;
      goto unumbers;
    case 'X':
      digits = large_digits;
    case 'x':
hex:
      base = 16;
unumbers:	// print unsigned numbers
      switch (sizemod) {
      case sizemod_none:
	ul = va_arg(ap, unsigned int);
        goto printul;

      case sizemod_h:
	ul = (unsigned short) va_arg(ap, unsigned int);
        goto printul;

      case sizemod_l:
	ul = va_arg(ap, unsigned long);
printul:
        do {
	  *(--p) = digits[ul % base];
          --precision;
        } while ((ul /= base) || precision > 0);
	break;

      case sizemod_ll:
	ull = va_arg(ap, unsigned long long);
printull:
        do {
	  *(--p) = digits[ull % base];
          --precision;
        } while ((ull /= base) || precision > 0);
      }

      break;

    case 's':
      {
	p = va_arg(ap, char *);
        int len = strlen(p);
        if (precision >= 0 && len > precision)
          len = precision;
        pend = p + len;
	break;
      }
    case '%':
      putc('%', pBuf);
      break;
    }

    if (width) {
      width -= pend - p;	// amount of padding
    }

    // Figure width of prefix or sign, subtract from padding.
    if (sign)
      width--;
    else if (sharpflag) {
      if (base == 8) {
        width--;	// for 0
      } else if (base == 16) {
        width -= 2;	// for 0x
      }
    }

    if (width < 0)
      width = 0;

    if (padchar != ' ')	// prefix/sign comes before 0 padding
      printPrefixSign(putc, pBuf, sign, sharpflag, base, digits);

    if (! leftAdjust)      // pad on the left
      while (width-- > 0)
        putc(padchar, pBuf);

    if (padchar == ' ')	// prefix/sign comes after space padding
      printPrefixSign(putc, pBuf, sign, sharpflag, base, digits);

    /* output the text */
    while (p != pend)
      putc(*p++, pBuf);

    while (width-- > 0) // add any padding on the right
      putc(padchar, pBuf);
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
  sprintf_putc(0, &theBuffer);	// add terminating NUL

  va_end(listp);

  return theBuffer.len - 1;
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
