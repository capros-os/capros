/*	$NetBSD: db_expr.c,v 1.4 1994/06/29 06:31:06 cgd Exp $	*/

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

#include <ddb/db_lex.h>
#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_expr.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>


bool
db_term(db_expr_t *valuep)
{
	int	t;

	t = db_read_token();
	if (t == tIDENT) {
	    if (!db_value_of_name(db_tok_string, valuep)) {
		db_error("Symbol not found\n");
		/*NOTREACHED*/
	    }
	    return (true);
	}
	if (t == tNUMBER) {
	    *valuep = (db_expr_t)db_tok_number;
	    return (true);
	}
	if (t == tDOT) {
	    *valuep = (db_expr_t)db_dot;
	    return (true);
	}
	if (t == tDOTDOT) {
	    *valuep = (db_expr_t)db_prev;
	    return (true);
	}
	if (t == tPLUS) {
	    *valuep = (db_expr_t) db_next;
	    return (true);
	}
	if (t == tDITTO) {
	    *valuep = (db_expr_t)db_last_addr;
	    return (true);
	}
	if (t == tDOLLAR) {
	    if (!db_get_variable(valuep))
		return (false);
	    return (true);
	}
	if (t == tLPAREN) {
	    if (!db_expression(valuep)) {
		db_error("Syntax error\n");
		/*NOTREACHED*/
	    }
	    t = db_read_token();
	    if (t != tRPAREN) {
		db_error("Syntax error\n");
		/*NOTREACHED*/
	    }
	    return (true);
	}
	db_unread_token(t);
	return (false);
}

bool
db_unary(db_expr_t *valuep)
{
	int	t;

	t = db_read_token();
	if (t == tMINUS) {
	    if (!db_unary(valuep)) {
		db_error("Syntax error\n");
		/*NOTREACHED*/
	    }
	    *valuep = -*valuep;
	    return (true);
	}
	if (t == tSTAR) {
	    /* indirection */
	    if (!db_unary(valuep)) {
		db_error("Syntax error\n");
		/*NOTREACHED*/
	    }
	    *valuep = db_get_value((db_addr_t)*valuep, sizeof(int), false);
	    return (true);
	}
	db_unread_token(t);
	return (db_term(valuep));
}

bool
db_mult_expr(db_expr_t *valuep)
{
	db_expr_t	lhs, rhs;
	int		t;

	if (!db_unary(&lhs))
	    return (false);

	t = db_read_token();
	while (t == tSTAR || t == tSLASH || t == tPCT || t == tHASH) {
	    if (!db_term(&rhs)) {
		db_error("Syntax error\n");
		/*NOTREACHED*/
	    }
	    if (t == tSTAR)
		lhs *= rhs;
	    else {
		if (rhs == 0) {
		    db_error("Divide by 0\n");
		    /*NOTREACHED*/
		}
		if (t == tSLASH)
		    lhs /= rhs;
		else if (t == tPCT)
		    lhs %= rhs;
		else
		    lhs = ((lhs+rhs-1)/rhs)*rhs;
	    }
	    t = db_read_token();
	}
	db_unread_token(t);
	*valuep = lhs;
	return (true);
}

bool
db_add_expr(db_expr_t *valuep)
{
	db_expr_t	lhs, rhs;
	int		t;

	if (!db_mult_expr(&lhs))
	    return (false);

	t = db_read_token();
	while (t == tPLUS || t == tMINUS) {
	    if (!db_mult_expr(&rhs)) {
		db_error("Syntax error\n");
		/*NOTREACHED*/
	    }
	    if (t == tPLUS)
		lhs += rhs;
	    else
		lhs -= rhs;
	    t = db_read_token();
	}
	db_unread_token(t);
	*valuep = lhs;
	return (true);
}

bool
db_shift_expr(db_expr_t *valuep)
{
	db_expr_t	lhs, rhs;
	int		t;

	if (!db_add_expr(&lhs))
	    return (false);

	t = db_read_token();
	while (t == tSHIFT_L || t == tSHIFT_R) {
	    if (!db_add_expr(&rhs)) {
		db_error("Syntax error\n");
		/*NOTREACHED*/
	    }
	    if (rhs < 0) {
		db_error("Negative shift amount\n");
		/*NOTREACHED*/
	    }
	    if (t == tSHIFT_L)
		lhs <<= rhs;
	    else {
		/* Shift right is unsigned */
		lhs = (unsigned) lhs >> rhs;
	    }
	    t = db_read_token();
	}
	db_unread_token(t);
	*valuep = lhs;
	return (true);
}

int
db_expression(db_expr_t *valuep)
{
	return (db_shift_expr(valuep));
}
