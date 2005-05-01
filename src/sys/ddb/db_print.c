/*	$NetBSD: db_print.c,v 1.4 1994/06/29 06:31:15 cgd Exp $	*/

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
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Miscellaneous printing.
 */
#include <arch-kerninc/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <ddb/db_examine.h>
#include <ddb/db_output.h>

extern db_expr_t	db_maxoff;

void
db_show_regs(db_expr_t adr/* addr */, int h_a/* have_addr */,
	     db_expr_t cnt/* count */, char * modf/* modif */)
{
#if 0
	int	(*func)();
#endif
	register struct db_variable *regp;
	db_expr_t	value, offset;
	const char *		name;

	for (regp = db_regs; regp < db_eregs; regp++) {
	    db_read_variable(regp, &value);
	    db_printf("%-12s%#10x", regp->name, value);
	    if (regp->isptr) {
	      db_find_xtrn_sym_and_offset((db_addr_t)value, &name, &offset);
	      if (name != 0 && offset <= db_maxoff && offset != value) {
		db_printf("\t%s", name);
		if (offset != 0)
		  db_printf("+%#r", offset);
	      }
	    }
	    db_printf("\n");
	}
	db_print_loc_and_inst(PC_REGS(OPTION_DDB_REGS));
}
