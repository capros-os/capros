/*	$NetBSD: db_variables.c,v 1.7 1994/10/09 08:56:28 mycroft Exp $	*/

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

#include <arch-kerninc/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_variables.h>
#include <ddb/db_command.h>
#include <ddb/db_expr.h>

#include <kerninc/util.h>

extern db_expr_t	db_maxoff;

extern long	db_radix;
extern long	db_max_width;
extern long	db_tab_stop_width;
extern long	db_max_line;

struct db_variable db_vars[] = {
	{ "radix",	&db_radix, FCN_NULL, false },
	{ "maxoff",	(long *)&db_maxoff, FCN_NULL, false },
	{ "maxwidth",	&db_max_width, FCN_NULL, false },
	{ "tabstops",	&db_tab_stop_width, FCN_NULL, false },
	{ "lines",	&db_max_line, FCN_NULL, false },
};
struct db_variable *db_evars = db_vars + sizeof(db_vars)/sizeof(db_vars[0]);

int
db_find_variable(db_variable **varp)
{
	int	t;
	struct db_variable *vp;

	t = db_read_token();
	if (t == tIDENT) {
	    for (vp = db_vars; vp < db_evars; vp++) {
		if (!strcmp(db_tok_string, vp->name)) {
		    *varp = vp;
		    return (1);
		}
	    }
	    for (vp = db_regs; vp < db_eregs; vp++) {
		if (!strcmp(db_tok_string, vp->name)) {
		    *varp = vp;
		    return (1);
		}
	    }
	}
	db_error("Unknown variable\n");
	/*NOTREACHED*/
	return 0;
}

int
db_get_variable(db_expr_t *valuep)
{
	struct db_variable *vp;

	if (!db_find_variable(&vp))
	    return (0);

	db_read_variable(vp, valuep);

	return (1);
}

int
db_set_variable(db_expr_t value)
{
	struct db_variable *vp;

	if (!db_find_variable(&vp))
	    return (0);

	db_write_variable(vp, &value);

	return (1);
}


void
db_read_variable(db_variable *vp, db_expr_t *valuep)
{
	int	(*func)(db_variable *vp, db_expr_t *valuep, int op);

	func = vp->fcn;

	if (func == FCN_NULL)
	    *valuep = *(vp->valuep);
	else
	    (*func)(vp, valuep, DB_VAR_GET);
}

void
db_write_variable(db_variable *vp, db_expr_t *valuep)
{
	int	(*func)(db_variable *vp, db_expr_t *valuep, int op);

	func = vp->fcn;

	if (func == FCN_NULL)
	    *(vp->valuep) = *valuep;
	else
	    (*func)(vp, valuep, DB_VAR_SET);
}


void
db_set_cmd(db_expr_t dbt, int it, db_expr_t dbet, char* chr)
{
	db_expr_t	value;
#if 0
	int	(*func)();
#endif
	struct db_variable *vp;
	int	t;

	t = db_read_token();
	if (t != tDOLLAR) {
	    db_error("Unknown variable\n");
	    /*NOTREACHED*/
	}
	if (!db_find_variable(&vp)) {
	    db_error("Unknown variable\n");
	    /*NOTREACHED*/
	}

	t = db_read_token();
	if (t != tEQ)
	    db_unread_token(t);

	if (!db_expression(&value)) {
	    db_error("No value\n");
	    /*NOTREACHED*/
	}
	if (db_read_token() != tEOL) {
	    db_error("?\n");
	    /*NOTREACHED*/
	}

	db_write_variable(vp, &value);
}
