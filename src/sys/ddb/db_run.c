/*	$NetBSD: db_run.c,v 1.7 1994/10/09 08:30:08 mycroft Exp $	*/

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

/* #define DB_DEBUG */
/*
 * Commands to run process.
 */
#include <arch-kerninc/db_machdep.h>

#include <ddb/db_run.h>
#include <ddb/db_lex.h>
#include <ddb/db_break.h>
#include <ddb/db_examine.h>
#include <ddb/db_access.h>
#include <ddb/db_watch.h>
#include <ddb/db_output.h>

#ifdef DB_DEBUG
#include <kerninc/MsgLog.h>
#endif

int	db_run_mode;
#define	STEP_NONE	0
#define	STEP_ONCE	1
#define	STEP_RETURN	2
#define	STEP_CALLT	3
#define	STEP_CONTINUE	4
#define STEP_INVISIBLE	5
#define	STEP_COUNT	6

bool	db_sstep_print;
int		db_loop_count;
int		db_call_depth;
int		db_inst_count;
int		db_load_count;
int		db_store_count;

bool
db_stop_at_pc(db_regs_t *regs, bool *is_breakpoint)
{
	register db_addr_t	pc;
	register db_breakpoint_t bkpt;

#ifdef DB_DEBUG
	printf("stop at pc 0x%08x, is_bpt='%c'\n",
		       PC_REGS(regs), (*is_breakpoint ? 'y' : 'n'));
#endif

	db_clear_single_step(regs);
	db_clear_breakpoints();
#ifdef OPTION_OPTION_DDB_WATCH
	db_clear_watchpoints();
#endif
	pc = PC_REGS(regs);

#ifdef	FIXUP_PC_AFTER_BREAK
	if (*is_breakpoint) {
	    /*
	     * Breakpoint trap.  Fix up the PC if the
	     * machine requires it.
	     */
	    FIXUP_PC_AFTER_BREAK
	    pc = PC_REGS(regs);
#ifdef DB_DEBUG
	  printf("Fixup bpt pc to 0x%08x\n", pc);
#endif
	}
#endif

	/*
	 * Now check for a breakpoint at this address.
	 */
	bkpt = db_find_breakpoint_here(pc);
	if (bkpt) {
#ifdef DB_DEBUG
	  printf("Really a bpt here\n");
#endif
	    if (--bkpt->count == 0) {
		bkpt->count = bkpt->init_count;
		*is_breakpoint = true;
		return (true);	/* stop here */
	    }
	} else if (*is_breakpoint) {
		PC_REGS(regs) += BKPT_SIZE;
		pc = PC_REGS(regs);
#ifdef DB_DEBUG
	  printf("Adjust pc to point past bpt, pc to 0x%08x\n", pc);
#endif
	}
		
	*is_breakpoint = false;

	if (db_run_mode == STEP_INVISIBLE) {
	    db_run_mode = STEP_CONTINUE;
	    return (false);	/* continue */
	}
	if (db_run_mode == STEP_COUNT) {
	    return (false); /* continue */
	}
	if (db_run_mode == STEP_ONCE) {
	    if (--db_loop_count > 0) {
		if (db_sstep_print) {
		    db_printf("\t\t");
		    db_print_loc_and_inst(pc);
		    db_printf("\n");
		}
		return (false);	/* continue */
	    }
	}
	if (db_run_mode == STEP_RETURN) {
	    db_expr_t ins = db_get_value(pc, sizeof(int), false);

	    /* continue until matching return */

	    if (!inst_trap_return(ins) &&
		(!inst_return(ins) || --db_call_depth != 0)) {
		if (db_sstep_print) {
		    if (inst_call(ins) || inst_return(ins)) {
			register int i;

			db_printf("[after %6d]     ", db_inst_count);
			for (i = db_call_depth; --i > 0; )
			    db_printf("  ");
			db_print_loc_and_inst(pc);
			db_printf("\n");
		    }
		}
		if (inst_call(ins))
		    db_call_depth++;
		return (false);	/* continue */
	    }
	}
	if (db_run_mode == STEP_CALLT) {
	    db_expr_t ins = db_get_value(pc, sizeof(int), false);

	    /* continue until call or return */

	    if (!inst_call(ins) &&
		!inst_return(ins) &&
		!inst_trap_return(ins)) {
		return (false);	/* continue */
	    }
	}
	db_run_mode = STEP_NONE;
	return (true);
}

void
db_restart_at_pc(db_regs_t *regs, bool watchpt)
{
	register db_addr_t pc = PC_REGS(regs);

	if ((db_run_mode == STEP_COUNT) ||
	    (db_run_mode == STEP_RETURN) ||
	    (db_run_mode == STEP_CALLT)) {
	    db_expr_t		ins;

	    /*
	     * We are about to execute this instruction,
	     * so count it now.
	     */

	    ins = db_get_value(pc, sizeof(int), false);
	    db_inst_count++;
	    db_load_count += inst_load(ins);
	    db_store_count += inst_store(ins);
#ifdef	SOFTWARE_SSTEP
	    /* XXX works on mips, but... */
	    if (inst_branch(ins) || inst_call(ins)) {
		ins = db_get_value(next_instr_address(pc,1),
				   sizeof(int), false);
		db_inst_count++;
		db_load_count += inst_load(ins);
		db_store_count += inst_store(ins);
	    }
#endif	/* SOFTWARE_SSTEP */
	}

	if (db_run_mode == STEP_CONTINUE) {
	    if (watchpt || db_find_breakpoint_here(pc)) {
		/*
		 * Step over breakpoint/watchpoint.
		 */
#ifdef DB_DEBUG
	      printf("stepping over bpt @ 0x%08x\n", pc);
#endif
		db_run_mode = STEP_INVISIBLE;
		db_set_single_step(regs);
	    } else {
		db_set_breakpoints();
#ifdef OPTION_OPTION_DDB_WATCH
		db_set_watchpoints();
#endif
	    }
	} else {
#ifdef DB_DEBUG
	      printf("db_restart setting single step bit, pc 0x%08x\n",
			     PC_REGS(regs));
#endif
	    db_set_single_step(regs);
	}
}

void
db_single_step(db_regs_t *regs)
{
	if (db_run_mode == STEP_CONTINUE) {
	    db_run_mode = STEP_INVISIBLE;
	    db_set_single_step(regs);
	}
}

#ifdef	SOFTWARE_SSTEP
/*
 *	Software implementation of single-stepping.
 *	If your machine does not have a trace mode
 *	similar to the vax or sun ones you can use
 *	this implementation, done for the mips.
 *	Just define the above conditional and provide
 *	the functions/macros defined below.
 *
 * extern bool
 *	inst_branch(),		returns true if the instruction might branch
 * extern unsigned
 *	branch_taken(),		return the address the instruction might
 *				branch to
 *	db_getreg_val();	return the value of a user register,
 *				as indicated in the hardware instruction
 *				encoding, e.g. 8 for r8
 *			
 * next_instr_address(pc,bd)	returns the address of the first
 *				instruction following the one at "pc",
 *				which is either in the taken path of
 *				the branch (bd==1) or not.  This is
 *				for machines (mips) with branch delays.
 *
 *	A single-step may involve at most 2 breakpoints -
 *	one for branch-not-taken and one for branch taken.
 *	If one of these addresses does not already have a breakpoint,
 *	we allocate a breakpoint and save it here.
 *	These breakpoints are deleted on return.
 */			
db_breakpoint_t	db_not_taken_bkpt = 0;
db_breakpoint_t	db_taken_bkpt = 0;

void
db_set_single_step(register db_regs_t *regs)
{
	db_addr_t pc = PC_REGS(regs);
	register unsigned	 inst, brpc;

	/*
	 *	User was stopped at pc, e.g. the instruction
	 *	at pc was not executed.
	 */
	inst = db_get_value(pc, sizeof(int), false);
	if (inst_branch(inst) || inst_call(inst)) {
	    extern unsigned getreg_val();

	    brpc = branch_taken(inst, pc, getreg_val, regs);
	    if (brpc != pc) {	/* self-branches are hopeless */
		db_taken_bkpt = db_set_temp_breakpoint(brpc);
	    }
	    pc = next_instr_address(pc,1);
	}
	pc = next_instr_address(pc,0);
	db_not_taken_bkpt = db_set_temp_breakpoint(pc);
}

void
db_clear_single_step(register db_regs_t *regs)
{
	if (db_taken_bkpt != 0) {
	    db_delete_temp_breakpoint(db_taken_bkpt);
	    db_taken_bkpt = 0;
	}
	if (db_not_taken_bkpt != 0) {
	    db_delete_temp_breakpoint(db_not_taken_bkpt);
	    db_not_taken_bkpt = 0;
	}
}

#endif	/* SOFTWARE_SSTEP */

extern int	db_cmd_loop_done;

/* single-step */
/*ARGSUSED*/
void
db_single_step_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		   db_expr_t count, char *modif)
{
	bool	print = false;

	if (count == -1)
	    count = 1;

	if (modif[0] == 'p')
	    print = true;

	db_run_mode = STEP_ONCE;
	db_loop_count = count;
	db_sstep_print = print;
	db_inst_count = 0;
	db_load_count = 0;
	db_store_count = 0;

	db_cmd_loop_done = 1;
}

/* trace and print until call/return */
/*ARGSUSED*/
void
db_trace_until_call_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
			db_expr_t cnt/* count */, char *modif)
{
	bool	print = false;

	if (modif[0] == 'p')
	    print = true;

	db_run_mode = STEP_CALLT;
	db_sstep_print = print;
	db_inst_count = 0;
	db_load_count = 0;
	db_store_count = 0;

	db_cmd_loop_done = 1;
}

/*ARGSUSED*/
void
db_trace_until_matching_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
			    db_expr_t cnt/* count */, char *modif)
{
	bool	print = false;

	if (modif[0] == 'p')
	    print = true;

	db_run_mode = STEP_RETURN;
	db_call_depth = 1;
	db_sstep_print = print;
	db_inst_count = 0;
	db_load_count = 0;
	db_store_count = 0;

	db_cmd_loop_done = 1;
}

/* continue */
/*ARGSUSED*/
void
db_continue_cmd(db_expr_t adr/* addr */, int hadr/* have_addr */,
		db_expr_t cnt/* count */, char *modif)
{
	if (modif[0] == 'c')
	    db_run_mode = STEP_COUNT;
	else
	    db_run_mode = STEP_CONTINUE;
	db_inst_count = 0;
	db_load_count = 0;
	db_store_count = 0;

	db_cmd_loop_done = 1;
}


bool
db_in_single_step()
{
  return(db_run_mode != STEP_NONE && db_run_mode != STEP_CONTINUE);
}

