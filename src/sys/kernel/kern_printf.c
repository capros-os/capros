/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* The bulk of this code originated as the Mach debugger printf()
 * code. While I could replace it with the equivalent code from
 * MsgLog::printf(), this version is well tested and mature.
 */
/*
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

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/KernStream.h>
#include <kerninc/IRQ.h>
#include <kerninc/Activity.h>
#include <eros/stdarg.h>

#ifdef OPTION_DDB
#include <arch-kerninc/db_machdep.h>
#include <ddb/db_command.h>
#endif

/*	$NetBSD: db_output.c,v 1.8 1994/06/29 22:41:41 deraadt Exp $	*/

#ifndef	DB_MAX_LINE
#define	DB_MAX_LINE		23	/* maximum line */
#define DB_MAX_WIDTH		80	/* maximum width */
#endif	/* DB_MAX_LINE */

#define DB_MIN_MAX_WIDTH	20	/* minimum max width */
#define DB_MIN_MAX_LINE		3	/* minimum max line */
#define CTRL(c)			((c) & 0x1f)

int	db_output_position = 0;		/* output column */
int	db_output_line = 0;		/* output line number */
int	db_last_non_space = 0;		/* last non-space character */
int	db_tab_stop_width = 8;		/* how wide are tab stops? */
#define	NEXT_TAB(i) \
	((((i) + db_tab_stop_width) / db_tab_stop_width) * db_tab_stop_width)
int	db_max_line = DB_MAX_LINE;	/* output max lines */
int	db_max_width = DB_MAX_WIDTH;	/* output line width */


extern void db_check_interrupt();

#define cngetc() kstream_dbg_stream->Get()
#define cnputc(c) kstream_nl_putc(c)

/*
 * Printf and character output for debugger.
 */

#define NBBY 8			/* for all modern machines */

/*
 * Printing
 */
long	db_radix = 10;

/*
 * End line if too long.
 */
void
db_end_line()
{
	if (db_output_position >= db_max_width)
	    printf("\n");
}

#ifdef OPTION_DDB
/*
 * Return output position
 */
int
db_print_position()
{
	return (db_output_position);
}
#endif

static const char small_digits[] = "0123456789abcdefx";
static const char large_digits[] = "0123456789ABCDEFX";

/*
 * Force pending whitespace.
 */
void
db_force_whitespace()
{
	int last_print = db_last_non_space;
	while (last_print < db_output_position) {
		cnputc(' ');
		last_print++;
	}
        while (last_print > db_output_position) {
        	cnputc('\b');
        	last_print--;
        }
	db_last_non_space = db_output_position;
}

#ifdef OPTION_DDB
static void
db_more()
{
	register  char *p;
	int quit_output = 0;

	if (kstream_debuggerIsActive == false) {
	  db_output_line = 0;
	  return;
	}

	for (p = "--db_more--"; *p; p++)
	    cnputc(*p);
	switch(cngetc()) {
	case ' ':
	    db_output_line = 0;
	    break;
	case 'q':
	case CTRL('c'):
	    db_output_line = 0;
	    quit_output = 1;
	    break;
	default:
	    db_output_line--;
	    break;
	}
	p = "\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b";
	while (*p)
	    cnputc(*p++);
	if (quit_output) {
	    db_abort_output();
	    /* NOTREACHED */
	}
}
#endif

/*
 * Output character.  Buffer whitespace.
 */
void
db_putchar(int c)
{
#ifdef OPTION_DDB
	if (db_max_line >= DB_MIN_MAX_LINE && db_output_line >= db_max_line-1)
	    db_more();
#endif
	if (c > ' ' && c <= '~') {
	    /*
	     * Printing character.
	     * If we have spaces to print, print them first.
	     * Use tabs if possible.
	     */
	    db_force_whitespace();
	    cnputc(c);
	    db_output_position++;
	    if (db_max_width >= DB_MIN_MAX_WIDTH
		&& db_output_position >= db_max_width-1) {
		/* auto new line */
		cnputc('\n');
		db_output_position = 0;
		db_last_non_space = 0;
		db_output_line++;
	    }
	    db_last_non_space = db_output_position;
	}
	else if (c == '\n') {
	    /* Return */
	    cnputc(c);
	    db_output_position = 0;
	    db_last_non_space = 0;
	    db_output_line++;
#ifdef OPTION_DDB
	    db_check_interrupt();
#endif
	}
	else if (c == '\t') {
	    /* assume tabs every 8 positions */
	    db_output_position = NEXT_TAB(db_output_position);
	}
	else if (c == ' ') {
	    /* space */
	    db_output_position++;
	}
	else if (c == '\b') {
	    /* backspace */
	    db_output_position--;
	}
	else if (c == '\007') {
	    /* bell */
	    cnputc(c);
	}
	/* other characters are assumed non-printing */
}

void
printPrefixSign(char sign, bool sharpflag, int base, const char * digits)
{
  if (sign)     // add sign
    db_putchar(sign);
  else
    // Add prefix.
    if (sharpflag) {
      if (base == 8) {
        db_putchar('0');
      } else if (base == 16) {
        db_putchar('0');
        db_putchar(digits[16]);      // 'x' or 'X'
      }
    }
}

enum size_modifier {
  sizemod_none = 0,
  sizemod_l,
  sizemod_ll
};

void
db_printf_guts(register const char *fmt, va_list ap)
{
  register char * p = 0;
  register int ch;
  unsigned long ul;
  unsigned long long ull;

  /* Smallest base is 8, which takes 1 character per 3 bits. 
  Add 2 for prefix "0x" and 1 for sign. */
  char buf[sizeof(long long) * NBBY / 3 + 3];

  for (; *fmt;) {
    if (*fmt != '%') {
      db_putchar(*fmt++);
      continue;
    }

    int base;
    char padchar = ' ';
    int width = 0;
    enum size_modifier sizemod = sizemod_none;
    bool ladjust = false;
    bool sharpflag = false;
    int precision = -1;	// so far, not present
    char sign = 0;
    const char * digits = small_digits;
    
    /* Process a conversion specification. */
    /* First the flags: */
  flags:
    fmt++;	// consume the '%' or flag

    switch (*fmt) {
      case '-': ladjust = true; goto flags;
      case '0': padchar = '0'; goto flags;
      case '#': sharpflag = true; goto flags;
    default: break;
    }
     
    /* See if what follows is a width specifier: */

    if (*fmt == '*') {
      fmt++;
      width = va_arg (ap, int);
      if (width < 0) {
        ladjust = !ladjust;
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
    default: goto nosize;
    }
    fmt++;
    nosize:

  switch (ch = *(unsigned char *)fmt++) {
  case 'c':
    db_putchar(va_arg(ap, int));
    break;
  case 's':
  {
    p = va_arg(ap, char *);
    int len = strlen(p);
    if (precision >= 0 && len > precision)
      len = precision;
    width -= len;
    if (!ladjust && width > 0)
      while (width--)
	db_putchar(padchar);
    while (len--)
      db_putchar(*p++);
    if (ladjust && width > 0)
      while (width--)
	db_putchar(padchar);
    break;
  }
  case 'r':
    base = db_radix;
    if (base < 8 || base > 16)
      base = 10;
    goto snumbers;
  case 'n':
    base = db_radix;
    if (base < 8 || base > 16)
      base = 10;
    goto unumbers;

  case 'd':
  case 'i':
    base = 10;
    // Signed numbers come here.
snumbers:
    switch (sizemod) {
      case sizemod_none:
        ul = va_arg(ap, unsigned int);
        break;

      default:	// to satisfy the compiler
      case sizemod_l:
        ul = va_arg(ap, unsigned long);
        break;

      case sizemod_ll:
        ull = va_arg(ap, unsigned long long);
        if (ull < 0) {
          sign = '-';
          ull = -(long long)ull;
        }
        goto printull;
    }
    if ((long)ul < 0) {
      sign = '-';
      ul = -(long)ul;
    }
    goto printul;

  case 'o':
    base = 8;
    goto unumbers;
  case 'u':
    base = 10;
    goto unumbers;
  case 'X':
    digits = large_digits;
    // fall through
  case 'x':
    base = 16;

    // Unsigned numbers come here.
unumbers:
    switch (sizemod) {
      case sizemod_none:
        ul = va_arg(ap, unsigned int);
        goto printul;

      case sizemod_l:
        ul = va_arg(ap, unsigned long);
printul:
        p = buf;
        do {
          *p++ = digits[ul % base];
          --precision;
        } while ((ul /= base) || precision > 0);
        break;

      case sizemod_ll:
        ull = va_arg(ap, unsigned long long);
printull:
        p = buf;
        do {
          *p++ = digits[ull % base];
          --precision;
        } while ((ull /= base) || precision > 0);
    }

    if (width) {
      width -= p - buf;	// amount of padding
    }

    // Figure width of prefix or sign, subtract from padding.
    if (sign)
      width--;
    else if (sharpflag) {
      if (base == 8) {
        width--;        // for 0
      } else if (base == 16) {
        width -= 2;     // for 0x
      }
    }

    if (width < 0)
      width = 0;

    if (padchar != ' ') // prefix/sign comes before 0 padding
      printPrefixSign(sign, sharpflag, base, digits);

    if (! ladjust)	// pad on the left
      while (width-- > 0)
	db_putchar(padchar);

    if (padchar == ' ') // prefix/sign comes after space padding
      printPrefixSign(sign, sharpflag, base, digits);

    do
      db_putchar(*--p);
    while (p != buf);

    while (width-- > 0)	// add any padding on the right
      db_putchar(padchar);

    break;

  default:
    db_putchar('%');
    /* FALLTHROUGH */
  case '%':
    db_putchar(ch);
  }
  }
}


/*VARARGS1*/
void
printf(const char *fmt, ...)
{
  va_list	listp;
  va_start(listp, fmt);

  // modified by hchen
  if (!kstream_debuggerIsActive)
  {
    irqFlags_t flags = local_irq_save();
    db_printf_guts (fmt, listp);
    local_irq_restore(flags);
  }
  else
  {
    db_printf_guts (fmt, listp);
  }    
  va_end(listp);
}

/*VARARGS1*/
void dprintf(unsigned shouldStop, const char* fmt, ...)
{
  va_list	listp;
  va_start(listp, fmt);

  // modified by hchen
  if (!kstream_debuggerIsActive)
  {
    irqFlags_t flags = local_irq_save();
    db_printf_guts (fmt, listp);
    local_irq_restore(flags);
  }
  else
  {
    db_printf_guts (fmt, listp);
  }
  va_end(listp);

  if (shouldStop) Debugger();
}

/*VARARGS1*/
void
fatal(const char *fmt, ...)
{
  va_list	listp;
  va_start(listp, fmt);

  /* modified by hchen to make MOPS happy */
  if (!kstream_debuggerIsActive)
  {
    irqFlags_t flags = local_irq_save();
    db_printf_guts (fmt, listp);
    local_irq_restore(flags);
  }
  else
  {
    db_printf_guts (fmt, listp);
  }
  va_end(listp);

#ifdef OPTION_DDB
  Debugger();
#else
#ifndef NDEBUG
  Activity * act = act_Current();
  if (act) proc_DumpFixRegs(act->context);
#endif
#if 0
  Debug::Backtrace(0, false);
  
  /* This routine will never return. I personally guarantee it! */
  Flush();
#endif
  halt('F');
#endif
}

void printOid(OID oid)
{
  printf("%#016llx", oid);
}
