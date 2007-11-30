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

/*
 * Put a number (base <= 16) in a buffer in reverse order; return an
 * optional length and a pointer to the NULL terminated (preceded?)
 * buffer.
 */
static char *
db_ksprintn(register unsigned long ul, register int base, register int *lenp)
{					/* A long in base 8, plus NULL. */
  static char buf[sizeof(long) * NBBY / 3 + 2];
  register char *p;

  p = buf;
  do {
    *++p = "0123456789abcdef"[ul % base];
  } while (ul /= base);
  if (lenp)
    *lenp = p - buf;
  return (p);
}

static char *
db_ll_ksprintn(register unsigned long long ull, register int base,
	    register int *lenp)
{
  static char buf[sizeof(long long) * NBBY / 3 + 2];
  register char *p;

  p = buf;
  do {
    *++p = "0123456789abcdef"[ull % base];
  } while (ull /= base);
  if (lenp)
    *lenp = p - buf;
  return (p);
}

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
db_printf_guts(register const char *fmt, va_list ap)
{
  register char *p;
  register int ch, n;
  unsigned long ul;
  unsigned long long ull;
  int base, lflag, tmp, width;
  char padc;
  int ladjust;
  int sharpflag;
  int neg;

  for (;;) {
    padc = ' ';
    width = 0;
    while ((ch = *(unsigned char *)fmt++) != '%') {
      if (ch == '\0')
	return;
      db_putchar(ch);
    }
    lflag = 0;
    ladjust = 0;
    sharpflag = 0;
    neg = 0;
  reswitch:	switch (ch = *(unsigned char *)fmt++) {
  case '0':
    padc = '0';
    goto reswitch;
  case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    for (width = 0;; ++fmt) {
      width = width * 10 + ch - '0';
      ch = *fmt;
      if (ch < '0' || ch > '9')
	break;
    }
    goto reswitch;
  case 'l':
    lflag = 1;
    goto reswitch;
  case '-':
    ladjust = 1;
    goto reswitch;
  case '#':
    sharpflag = 1;
    goto reswitch;
  case 'b':
    ul = va_arg(ap, int);
    p = va_arg(ap, char *);
    for (p = db_ksprintn(ul, *p++, NULL); (ch = *p--);)
      db_putchar(ch);

    if (!ul)
      break;

    for (tmp = 0; (n = *p++);) {
      if (ul & (1 << (n - 1))) {
	db_putchar(tmp ? ',' : '<');
	for (; (n = *p) > ' '; ++p)
	  db_putchar(n);
	tmp = 1;
      } else
	for (; *p > ' '; ++p);
    }
    if (tmp)
      db_putchar('>');
    break;
  case '*':
    width = va_arg (ap, int);
    if (width < 0) {
      ladjust = !ladjust;
      width = -width;
    }
    goto reswitch;
  case 'c':
    db_putchar(va_arg(ap, int));
    break;
  case 's':
    p = va_arg(ap, char *);
    width -= strlen (p);
    if (!ladjust && width > 0)
      while (width--)
	db_putchar (padc);
    while ((ch = *p++))
      db_putchar(ch);
    if (ladjust && width > 0)
      while (width--)
	db_putchar (padc);
    break;
  case 'r':
    ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
    if ((long)ul < 0) {
      neg = 1;
      ul = -(long)ul;
    }
    base = db_radix;
    if (base < 8 || base > 16)
      base = 10;
    goto number;
  case 'n':
    ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
    base = db_radix;
    if (base < 8 || base > 16)
      base = 10;
    goto number;
  case 'd':
    ul = lflag ? va_arg(ap, long) : va_arg(ap, int);
    if ((long)ul < 0) {
      neg = 1;
      ul = -(long)ul;
    }
    base = 10;
    goto number;
  case 'o':
    ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
    base = 8;
    goto number;
  case 'u':
    ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
    base = 10;
    goto number;
  case 'U':
    ull = va_arg(ap, unsigned long long);
    base = 10;
    goto ll_number;
  case 'X':
    ull = va_arg(ap, unsigned long long);
    base = 16;
    goto ll_number;
  case 'z':
    ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
    if ((long)ul < 0) {
      neg = 1;
      ul = -(long)ul;
    }
    base = 16;
    goto number;
  case 'x':
    ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
    base = 16;
#if 0
  number:			p = (char *)db_ksprintn(ul, base, &tmp);
    if (sharpflag && ul != 0) {
      if (base == 8)
	tmp++;
      else if (base == 16)
	tmp += 2;
    }
    if (neg)
      tmp++;

    if (!ladjust && width && (width -= tmp) > 0)
      while (width--)
	db_putchar(padc);
    if (neg)
      db_putchar ('-');
    if (sharpflag && ul != 0) {
      if (base == 8) {
	db_putchar ('0');
      } else if (base == 16) {
	db_putchar ('0');
	db_putchar ('x');
      }
    }
    if (ladjust && width && (width -= tmp) > 0)
      while (width--)
	db_putchar(padc);

    while ((ch = *p--))
      db_putchar(ch);
    break;

#endif
  number:                 ull = ul;
  ll_number:		p = (char *)db_ll_ksprintn(ull, base, &tmp);
    if (sharpflag && ull != 0) {
      if (base == 8)
	tmp++;
      else if (base == 16)
	tmp += 2;
    }
    if (neg)
      tmp++;

    if (!ladjust && width && (width -= tmp) > 0)
      while (width--)
	db_putchar(padc);
    if (neg)
      db_putchar ('-');
    if (sharpflag && ull != 0) {
      if (base == 8) {
	db_putchar ('0');
      } else if (base == 16) {
	db_putchar ('0');
	db_putchar ('x');
      }
    }
    if (ladjust && width && (width -= tmp) > 0)
      while (width--)
	db_putchar(padc);

    while ((ch = *p--))
      db_putchar(ch);
    break;
  default:
    db_putchar('%');
    if (lflag)
      db_putchar('l');
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
    irq_DISABLE();
    db_printf_guts (fmt, listp);
    irq_ENABLE();
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
    irq_DISABLE();
    db_printf_guts (fmt, listp);
    irq_ENABLE();
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
    irq_DISABLE();
    db_printf_guts (fmt, listp);
    irq_ENABLE();
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
  printf("0x%08x%08x", (uint32_t) (oid >> 32), (uint32_t) oid);
}

void printCount(ObCount count)
{
  printf("0x%08x", count);
}
