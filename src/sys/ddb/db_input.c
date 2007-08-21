/*	$NetBSD: db_input.c,v 1.6 1994/10/26 17:57:50 mycroft Exp $	*/

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
 *
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include <arch-kerninc/db_machdep.h>

#include <kerninc/KernStream.h>

#include <ddb/db_output.h>
#include <ddb/db_command.h>

extern int db_inputchar(int c);
extern void db_putchar(int c);

/*
 * Character input and editing.
 */


/*
 * We don't track output position while editing input,
 * since input always ends with a new-line.  We just
 * reset the line position at the end.
 */
char *	db_lbuf_start;	/* start of input line buffer */
char *	db_lbuf_end;	/* end of input line buffer */
char *	db_lc;		/* current character */
char *	db_le;		/* one past last character */

#define	CTRL(c)		((c) & 0x1f)
#define	isspace(c)	((c) == ' ' || (c) == '\t')
#define	BLANK		' '
#define	BACKUP		'\b'

#define cnputc(c) kstream_nl_putc(c)
#define cngetc() kstream_dbg_stream->Get()

int cnmaygetc ()
{
	return (-1);
}

void
db_putstring(char *s, int count)
{
	while (--count >= 0)
	    cnputc(*s++);
}

void
db_putnchars(int c, int count)
{
	while (--count >= 0)
	    cnputc(c);
}

/*
 * Delete N characters, forward or backward
 */
#define	DEL_FWD		0
#define	DEL_BWD		1
void
db_delete(int n, int bwd)
{
	register char *p;

	if (bwd) {
	    db_lc -= n;
	    db_putnchars(BACKUP, n);
	}
	for (p = db_lc; p < db_le-n; p++) {
	    *p = *(p+n);
	    cnputc(*p);
	}
	db_putnchars(BLANK, n);
	db_putnchars(BACKUP, db_le - db_lc);
	db_le -= n;
}

/* returns true at end-of-line */
int
db_inputchar(int c)
{
	switch (c) {
	    case CTRL('b'):
		/* back up one character */
		if (db_lc > db_lbuf_start) {
		    cnputc(BACKUP);
		    db_lc--;
		}
		break;
	    case CTRL('f'):
		/* forward one character */
		if (db_lc < db_le) {
		    cnputc(*db_lc);
		    db_lc++;
		}
		break;
	    case CTRL('a'):
		/* beginning of line */
		while (db_lc > db_lbuf_start) {
		    cnputc(BACKUP);
		    db_lc--;
		}
		break;
	    case CTRL('e'):
		/* end of line */
		while (db_lc < db_le) {
		    cnputc(*db_lc);
		    db_lc++;
		}
		break;
	    case CTRL('h'):
	    case 0177:
		/* erase previous character */
		if (db_lc > db_lbuf_start)
		    db_delete(1, DEL_BWD);
		break;
	    case CTRL('d'):
		/* erase next character */
		if (db_lc < db_le)
		    db_delete(1, DEL_FWD);
		break;
	    case CTRL('k'):
		/* delete to end of line */
		if (db_lc < db_le)
		    db_delete(db_le - db_lc, DEL_FWD);
		break;
	    case CTRL('t'):
		/* twiddle last 2 characters */
		if (db_lc >= db_lbuf_start + 2) {
		    c = db_lc[-2];
		    db_lc[-2] = db_lc[-1];
		    db_lc[-1] = c;
		    cnputc(BACKUP);
		    cnputc(BACKUP);
		    cnputc(db_lc[-2]);
		    cnputc(db_lc[-1]);
		}
		break;
	    case CTRL('r'):
		db_putstring("^R\n", 3);
		if (db_le > db_lbuf_start) {
		    db_putstring(db_lbuf_start, db_le - db_lbuf_start);
		    db_putnchars(BACKUP, db_le - db_lc);
		}
		break;
	    case '\n':
	    case '\r':
		*db_le++ = c;
		return (1);
	    default:
		if (db_le == db_lbuf_end) {
		    cnputc('\007');
		}
		else if (c >= ' ' && c <= '~') {
		    register char *p;

		    for (p = db_le; p > db_lc; p--)
			*p = *(p-1);
		    *db_lc++ = c;
		    db_le++;
		    cnputc(c);
		    db_putstring(db_lc, db_le - db_lc);
		    db_putnchars(BACKUP, db_le - db_lc);
		}
		break;
	}
	return (0);
}

int
db_readline(char *lstart, int lsize)
{
	db_force_whitespace();	/* synch output position */

	db_lbuf_start = lstart;
	db_lbuf_end   = lstart + lsize;
	db_lc = lstart;
	db_le = lstart;

	while(!db_inputchar(cngetc()))
	  continue;

	db_putchar('\n');	/* synch output position */
	db_putchar('\r');	/* synch output position */

	*db_le = 0;
	return (db_le - db_lbuf_start);
}

void
db_check_interrupt()
{
	register int	c;

	c = cnmaygetc();
	switch (c) {
	    case -1:		/* no character */
		return;

	    case CTRL('c'):
		db_abort_output();
		/*NOTREACHED*/

	    case CTRL('s'):
		do {
		    c = cnmaygetc();
		    if (c == CTRL('c')) {
			db_abort_output();
			/*NOTREACHED*/
		    }
		} while (c != CTRL('q'));
		break;

	    default:
		/* drop on floor */
		break;
	}
}

