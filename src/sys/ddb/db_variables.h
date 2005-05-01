/*	$NetBSD: db_variables.h,v 1.4 1994/06/29 06:31:24 cgd Exp $	*/

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

#ifndef	_DB_VARIABLES_H_
#define	_DB_VARIABLES_H_

/*
 * Debugger variables.
 */
typedef struct db_variable db_variable;

struct db_variable {
	char	*name;		/* Name of variable */
	long	*valuep;	/* value of variable */
				/* function to call when reading/writing */
	int	(*fcn)(db_variable *vp, db_expr_t *valuep, int op);
#define DB_VAR_GET	0
#define DB_VAR_SET	1
        bool    isptr;		/* true if value might be a pointer */
};

#if 0
#define	FCN_NULL	((int (*)())0)
#else
#define FCN_NULL 0
#endif

extern struct db_variable	db_vars[];	/* debugger variables */
extern struct db_variable	*db_evars;
extern struct db_variable	db_regs[];	/* machine registers */
extern struct db_variable	*db_eregs;

extern int db_get_variable(db_expr_t *valuep);
extern void db_read_variable(db_variable *vp, db_expr_t *valuep);
extern void db_write_variable(db_variable *vp, db_expr_t *valuep);
#endif	/* _DB_VARIABLES_H_ */
