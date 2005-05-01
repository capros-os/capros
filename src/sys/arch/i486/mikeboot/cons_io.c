/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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


#include <eros/target.h>
#include <eros/stdarg.h>
#include <eros/i486/io.h>
#include <kerninc/BootInfo.h>
#include "boot-asm.h"
#include "boot.h"

#define K_RDWR 		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS 	0x64		/* keyboard status */
#define K_CMD	 	0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL 	0x01		/* output buffer full */
#define K_IBUF_FUL 	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   enable clock line */

/*
 * Gate A20 for high memory
 */
unsigned char	x_20 = KB_A20;
void
gateA20()
{
#ifdef	IBM_L40
	outb(0x92, 0x2);
#else	/* IBM_L40 */
	while (inb(K_STATUS) & K_IBUF_FUL);
	while (inb(K_STATUS) & K_OBUF_FUL)
		(void)inb(K_RDWR);

	outb(KC_CMD_WOUT, K_CMD);
	while (inb(K_STATUS) & K_IBUF_FUL);
	outb(x_20, K_RDWR);
	while (inb(K_STATUS) & K_IBUF_FUL);
#endif	/* IBM_L40 */
}

static char hexdigits[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

int
printf(const char *fmt, ...)
{
  extern void reset_twiddle();
  extern void putchar(int);

  va_list ap;

  va_start(ap, fmt);

  reset_twiddle();

  for( ; *fmt; fmt++) {
    int rightAdjust = 1;
    char fillchar = ' ';
    uint32_t width = 0;
    char sign = 0;
    /* largest thing we might convert fits in 16 digits: */
    char buf[16];
    char *pend = &buf[16];
    char *p = pend;
    
    if (*fmt != '%') {
      putchar(*fmt);
      continue;
    }

    fmt++;

    /* check for left adjust.. */
    if (*fmt == '-') {
      rightAdjust = 0;
      fmt++;
    }
      
    /* we just saw a format character.  See if what follows
     * is a width specifier:
     */

    if (*fmt == '0')
      fillchar = '0';

    while (*fmt && *fmt >= '0' && *fmt <= '9') {
      width *= 10;
      width += (*fmt - '0');
      fmt++;
    }
    
    
  more:
    switch (*fmt) {
    case 'l':
      fmt++;
      goto more;
    case 'c':
      {
	long c;
	c = va_arg(ap, long);
	*(--p) = (char) c;
	break;
      }	    
    case 'd':
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

	  if (l == TARGET_LONG_MIN)
	    ul = ((unsigned long) TARGET_LONG_MAX) + 1ul;

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
	    *(--p) = hexdigits[ul & 0xfu];
	    ul = ul / 16;
	  }
	}

	break;
      }
#if 0
    case 'X':
      {
	unsigned long long ull;
	    
	ull = va_arg(ap, unsigned long long);
	      
	if (ull == 0) {
	  *(--p) = '0';
	}
	else {
	  while(ull) {
	    *(--p) = hexdigits[ull & 0xfu];
	    ull = ull / 16;
	  }
	}

	break;
      }
#endif
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

    {
      uint32_t len = pend - p;

      if (sign)
	len++;

      /* do padding with initial spaces for right justification: */
      if (width && rightAdjust && len < width) {
	while (len < width) {
	  putchar(fillchar);
	  width--;
	}
      }

      if (sign)
	putchar('-');
    
      /* output the text */
      while (p != pend)
	putchar(*p++);
    
      /* do padding with initial spaces for left justification: */
      if (width && rightAdjust == 0 && len < width) {
	while (len < width) {
	  putchar(fillchar);
	  width--;
	}
      }
    }
  }

  va_end(ap);

  return 0;
}

void
putchar(int c)
{
  extern void putc(int);
  
  if (c == '\n')
    putc('\r');
  putc(c);
}

int
getchar(in_buf)
	int in_buf;
{
  extern int getc();
  int c;

loop:
  if ((c=getc()) == '\r')
    c = '\n';
  if (c == '\b') {
    if (in_buf != 0) {
      putchar('\b');
      putchar(' ');
    } else {
      goto loop;
    }
  }
  putchar(c);
  return(c);
}

void
waitkbd()
{
  extern int getc();

  (void) getc();
}

static int tw_on;
static int tw_pos;
static char tw_chars[] = "|/-\\";

void
reset_twiddle()
{
	if (tw_on)
		putchar('\b');
	tw_on = 0;
	tw_pos = 0;
}

void
twiddle()
{
	if (tw_on)
		putchar('\b');
	else
		tw_on = 1;
	putchar(tw_chars[tw_pos++]);
	tw_pos %= (sizeof(tw_chars) - 1);
}

/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:35:57  rpd
 *	io.c,v 1.10 1994/11/07 11:26:29 davidg Exp
 */
