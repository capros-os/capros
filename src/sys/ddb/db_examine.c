/*	$NetBSD: db_examine.c,v 1.9 1994/11/17 04:51:50 gwr Exp $	*/

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

#include <arch-kerninc/db_machdep.h>		/* type definitions */
#include <limits.h>

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_access.h>
#include <ddb/db_expr.h>
#include <ddb/db_examine.h>

#include <kerninc/util.h>

char	db_examine_format[TOK_STRING_SIZE] = "x";

extern	db_addr_t db_disasm(db_addr_t, bool);
/* instruction disassembler */

extern void db_examine(register db_addr_t addr,
		       char *fmt,		/* format string */
		       int count		/* repeat count */
		       );

extern void
db_search(db_addr_t addr, int size,
	  db_expr_t value, db_expr_t mask, unsigned int count);
/*
 * Examine (print) data.  Syntax is:
 *		x/[bhl][cdiorsuxz]*
 * For example, the command:
 *  	x/bxxxx
 * should print:
 *  	address:  01  23  45  67
 */
/*ARGSUSED*/
void
db_examine_cmd(db_expr_t addr, int ha/* have_addr */,
	       db_expr_t count, char *modif)
{
	if (modif[0] != '\0')
		strcpy(db_examine_format, modif);
		/* else use the same format as previously */

	if (count == -1)
		count = 1;

	db_examine((db_addr_t) addr, db_examine_format, count);
}

void db_examine(register db_addr_t addr,
	   char *fmt,		/* format string */
	   int count		/* repeat count */
	   )
{
	int		c;
	db_expr_t	value;
	int		size;
	int		width;
	int		space;
	char *		fp;

	printf("Examining 0x%08x\n", addr);
	while (--count >= 0) {
		fp = fmt;
		size = 4;
		width = 8;
		space = 4;
		while ((c = *fp++) != 0) {
			if (db_print_position() == 0) {
				/* Always print the address. */
				db_printsym(addr, DB_STGY_ANY);
				db_printf(":\t");
				db_prev = addr;
			}
			switch (c) {
			case 'b':	/* byte */
				size = 1;
				width = 1;
				space = 3;
				break;
			case 'h':	/* half-word */
				size = 2;
				width = 4;
				break;
			case 'l':	/* long-word */
				size = 4;
				width = 8;
				break;
			case 'a':	/* address */
				db_printf("= 0x%x\n", addr);
				addr += size;
				break;
			case 'r':	/* signed, current radix */
				value = db_get_value(addr, size, true);
				addr += size;
				db_printf("%-*r", width, value);
				break;
			case 'x':	/* unsigned hex */
				value = db_get_value(addr, size, false);
				addr += size;
				db_printf("0x%0*x", width, value);
				break;
			case 'p':	/* procedure */
				value = db_get_value(addr, size, false);
				db_printsym(value, DB_STGY_ANY);
				addr += size;
				db_printf("\n", width, value);
				break;
			case 'z':	/* signed hex */
				value = db_get_value(addr, size, true);
				addr += size;
				db_printf("0x%-*z", width, value);
				break;
			case 'd':	/* signed decimal */
				value = db_get_value(addr, size, true);
				addr += size;
				db_printf("%-*d", width, value);
				break;
			case 'u':	/* unsigned decimal */
				value = db_get_value(addr, size, false);
				addr += size;
				db_printf("%-*u", width, value);
				break;
			case 'o':	/* unsigned octal */
				value = db_get_value(addr, size, false);
				addr += size;
				db_printf("%-*o", width, value);
				break;
			case 'c':	/* character */
				value = db_get_value(addr, 1, false);
				addr += 1;
				if (value >= ' ' && value <= '~')
					db_printf("%c", value);
				else
					db_printf("\\%03o", value);
				break;
			case 's':	/* null-terminated string */
				for (;;) {
					value = db_get_value(addr, 1, false);
					addr += 1;
					if (value == 0)
						break;
					if (value >= ' ' && value <= '~')
						db_printf("%c", value);
					else
						db_printf("\\%03o", value);
				}
				break;
			case 'i':	/* instruction */
				addr = db_disasm(addr, false);
				break;
			case 'I':	/* instruction, alternate form */
				addr = db_disasm(addr, true);
				break;
			default:
				break;
			}
			db_printf("%*s", space, "");
			if (db_print_position() != 0)
				db_end_line();
		}
	}
	db_next = addr;
}

/*
 * Print value.
 */
char	db_print_format = 'x';

/*ARGSUSED*/
void
db_print_cmd(db_expr_t addr, int ha/* have_addr */,
	     db_expr_t co/* count */, char *modif)
{
	db_expr_t	value;

	if (modif[0] != '\0')
		db_print_format = modif[0];

	switch (db_print_format) {
	case 'a':
		db_printsym((db_addr_t)addr, DB_STGY_ANY);
		break;
	case 'r':
		db_printf("%11r", addr);
		break;
	case 'x':
		db_printf("%8x", addr);
		break;
	case 'z':
		db_printf("%8z", addr);
		break;
	case 'd':
		db_printf("%11d", addr);
		break;
	case 'u':
		db_printf("%11u", addr);
		break;
	case 'o':
		db_printf("%16o", addr);
		break;
	case 'c':
		value = addr & 0xFF;
		if (value >= ' ' && value <= '~')
			db_printf("%c", value);
		else
			db_printf("\\%03o", value);
		break;
	}
	db_printf("\n");
}

void
db_print_loc_and_inst(db_addr_t loc)
{
	db_printf("0x%08x\t", loc);
	db_printsym(loc, DB_STGY_PROC);
	db_printf(":\t");
	(void) db_disasm(loc, false);
}

/*
 * Search for a value in memory.
 * Syntax: search [/bhl] addr value [mask] [,count]
 */
void
db_search_cmd(db_expr_t det, int it, db_expr_t dtt, char* chr)
{
	int		t;
	db_addr_t	addr;
	int		size;
	db_expr_t	value;
	db_expr_t	mask;
	db_expr_t	count;

	t = db_read_token();
	if (t == tSLASH) {
		t = db_read_token();
		if (t != tIDENT) {
			bad_modifier:
			db_printf("Bad modifier\n");
			db_flush_lex();
			return;
		}

		if (!strcmp(db_tok_string, "b"))
			size = 1;
		else if (!strcmp(db_tok_string, "h"))
			size = 2;
		else if (!strcmp(db_tok_string, "l"))
			size = 4;
		else
			goto bad_modifier;
	} else {
		db_unread_token(t);
		size = 4;
	}

	if (!db_expression((db_expr_t *) &addr)) {
		db_printf("Address missing\n");
		db_flush_lex();
		return;
	}

	if (!db_expression(&value)) {
		db_printf("Value missing\n");
		db_flush_lex();
		return;
	}

	if (!db_expression(&mask))
		mask = 0xffffffff;

	t = db_read_token();
	if (t == tCOMMA) {
		if (!db_expression(&count)) {
			db_printf("Count missing\n");
			db_flush_lex();
			return;
		}
	} else {
		db_unread_token(t);
		count = INT_MAX; /* effectively forever */
	}
	db_skip_to_eol();

	db_search(addr, size, value, mask, (unsigned int)count);
}

void
db_search(register db_addr_t addr, int size,
	  db_expr_t value, db_expr_t mask, unsigned int count)
{
	while (count-- != 0) {
		db_prev = addr;
		if ((db_get_value(addr, size, false) & mask) == value)
			break;
		addr += size;
	}
	db_next = addr;
}
