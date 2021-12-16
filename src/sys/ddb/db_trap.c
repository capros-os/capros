/*	$NetBSD: db_trap.c,v 1.8 1994/12/02 06:07:37 gwr Exp $	*/

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
 * Trap entry point to kernel debugger.
 */
#include <arch-kerninc/db_machdep.h>

#include <ddb/db_run.h>
#include <ddb/db_command.h>
#include <ddb/db_break.h>
#include <ddb/db_examine.h>
#include <ddb/db_output.h>

extern  void
db_activity_print_cmd(db_expr_t addr, int have_addr,
                    db_expr_t /* count */, char * /* modif */);

void
db_trap(int type, int code)
{
	bool	bkpt;
	bool	watchpt;

	bkpt = IS_BREAKPOINT_TRAP(type, code);
	watchpt = IS_WATCHPOINT_TRAP(type, code);

	if (db_stop_at_pc(OPTION_DDB_REGS, &bkpt)) {
	    if (db_inst_count) {
		db_printf("After %d instructions (%d loads, %d stores),\n",
			  db_inst_count, db_load_count, db_store_count);
	    }

	    // db_activity_print_cmd(0, 0, 0, 0);

	    if (bkpt)
		db_printf("Breakpoint at ");
	    else if (watchpt)
		db_printf("Watchpoint at ");
	    else
		db_printf("Stopped at ");
	    db_dot = PC_REGS(OPTION_DDB_REGS);
	    db_print_loc_and_inst(db_dot);

	    db_command_loop();
	}

	db_restart_at_pc(OPTION_DDB_REGS, watchpt);
}

