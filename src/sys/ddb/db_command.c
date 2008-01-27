/*	$NetBSD: db_command.c,v 1.13 1994/10/09 08:29:59 mycroft Exp $	*/

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

/*
 * Command dispatcher.
 */

#include <arch-kerninc/db_machdep.h>		/* type definitions */
#include <arch-kerninc/setjmp.h>
#include <kerninc/Machine.h>
#include <kerninc/Activity.h>

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_expr.h>

/*#include <kerninc/util.h>*/
extern char *strcpy(char *c1, const char *c2);

/*
 * Exported global variables
 */
bool	db_cmd_loop_done;
jmp_buf	* db_recover = 0;

db_addr_t	db_dot;		/* current location */
db_addr_t	db_last_addr;	/* last explicit address typed */
db_addr_t	db_prev;	/* last address examined
					   or written */
db_addr_t	db_next;	/* next address to be examined
					   or written */

/*
 * if 'ed' style: 'dot' is set at start of last item printed,
 * and '+' points to next line.
 * Otherwise: 'dot' points to next item, '..' points to last.
 */
bool	db_ed_style = true;

/*
 * Utility routine - discard tokens through end-of-line.
 */
void
db_skip_to_eol()
{
	int	t;
	do {
	    t = db_read_token();
	} while (t != tEOL);
}

/*
 * Results of command search.
 */
#define	CMD_UNIQUE	0
#define	CMD_FOUND	1
#define	CMD_NONE	2
#define	CMD_AMBIGUOUS	3
#define	CMD_HELP	4

/*
 * Search for command prefix.
 */
int
db_cmd_search(char *name, struct db_command *table, struct db_command **cmdp /* out */)
{
	struct db_command	*cmd;
	int			result = CMD_NONE;

	for (cmd = table; cmd->name != 0; cmd++) {
	    register char *lp;
	    register char *rp;
	    register int  c;

	    lp = name;
	    rp = cmd->name;
	    while ((c = *lp) == *rp) {
		if (c == 0) {
		    /* complete match */
		    *cmdp = cmd;
		    return (CMD_UNIQUE);
		}
		lp++;
		rp++;
	    }
	    if (c == 0) {
		/* end of name, not end of command -
		   partial match */
		if (result == CMD_FOUND) {
		    result = CMD_AMBIGUOUS;
		    /* but keep looking for a full match -
		       this lets us match single letters */
		}
		else {
		    *cmdp = cmd;
		    result = CMD_FOUND;
		}
	    }
	}
	if (result == CMD_NONE) {
	    /* check for 'help' */
		if (name[0] == 'h' && name[1] == 'e'
		    && name[2] == 'l' && name[3] == 'p')
			result = CMD_HELP;
	}
	return (result);
}

void
db_cmd_list(struct db_command *table)
{
	int i = 0;
	register struct db_command *cmd;

	for (cmd = table; cmd->name != 0; cmd++) {
	    db_printf("%-12s", cmd->name);
	    i++;
	    if ((i % 6) == 0)
	      db_printf("\n");
	    db_end_line();
	}
}

void
db_command(struct db_command **last_cmdp /* IN_OUT */, struct db_command *cmd_table)
{
	struct db_command	*cmd;
	int		t;
	char		modif[TOK_STRING_SIZE];
	db_expr_t	addr, count;
	bool		have_addr = false;
	int		result;

	t = db_read_token();
	if (t == tEOL) {
	    /* empty line repeats last command, at 'next' */
	    cmd = *last_cmdp;
	    addr = (db_expr_t)db_next;
	    have_addr = false;
	    count = 1;
	    modif[0] = '\0';
	}
	else if (t == tEXCL) {
	    void db_fncall(db_expr_t, int, db_expr_t, char*);
	    db_fncall(0,0,0,0);
	    return;
	}
	else if (t == tQUERY) {
	    extern void db_sym_match(const char *symstr);
	    t = db_read_token();
	    db_sym_match(db_tok_string);
	    return;
	}
	else if (t != tIDENT) {
	    db_printf("?\n");
	    db_flush_lex();
	    return;
	}
	else {
	    /*
	     * Search for command
	     */
	    while (cmd_table) {
		result = db_cmd_search(db_tok_string,
				       cmd_table,
				       &cmd);
		switch (result) {
		    case CMD_NONE:
			db_printf("No such command\n");
			db_flush_lex();
			return;
		    case CMD_AMBIGUOUS:
			db_printf("Ambiguous\n");
			db_flush_lex();
			return;
		    case CMD_HELP:
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    default:
			break;
		}
		if ((cmd_table = cmd->more) != 0) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    }
		}
	    }

	    if ((cmd->flag & CS_OWN) == 0) {
		/*
		 * Standard syntax:
		 * command [/modifier] [addr] [,count]
		 */
		t = db_read_token();
		if (t == tSLASH) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_printf("Bad modifier\n");
			db_flush_lex();
			return;
		    }
		    strcpy(modif, db_tok_string);
		}
		else {
		    db_unread_token(t);
		    modif[0] = '\0';
		}

		if (db_expression(&addr)) {
		    db_dot = (db_addr_t) addr;
		    db_last_addr = db_dot;
		    have_addr = true;
		}
		else {
		    addr = (db_expr_t) db_dot;
		    have_addr = false;
		}
		t = db_read_token();
		if (t == tCOMMA) {
		    if (!db_expression(&count)) {
			db_printf("Count missing\n");
			db_flush_lex();
			return;
		    }
		}
		else {
		    db_unread_token(t);
		    count = -1;
		}
		if ((cmd->flag & CS_MORE) == 0) {
		    db_skip_to_eol();
		}
	    }
	}
	*last_cmdp = cmd;
	if (cmd != 0) {
	    /*
	     * Execute the command.
	     */
	    (*cmd->fcn)(addr, have_addr, count, modif);

	    if (cmd->flag & CS_SET_DOT) {
		/*
		 * If command changes dot, set dot to
		 * previous address displayed (if 'ed' style).
		 */
		if (db_ed_style) {
		    db_dot = db_prev;
		}
		else {
		    db_dot = db_next;
		}
	    }
	    else {
		/*
		 * If command does not change dot,
		 * set 'next' location to be the same.
		 */
		db_next = db_dot;
	    }
	}
}

#if 0
/*ARGSUSED*/
void
db_map_print_cmd(db_expr_t /* addr */, int /* have_addr */,
		 db_expr_t /* count */, char * /* modif */)
{
        extern void	_vm_map_print(db_expr_t, bool,
				      void (*)(const char *, ...));
        bool full = false;
        
        if (modif[0] == 'f')
                full = true;

        _vm_map_print(addr, full, db_printf);
}
#endif

/*ARGSUSED*/
#if 0
void
db_object_print_cmd(db_expr_t /* addr */, int /* have_addr */,
		    db_expr_t /* count */,
		    char * /* modif */)
{
        extern void	_vm_object_print(db_expr_t, bool,
				      void (*)(const char *, ...));
        bool full = false;
        
        if (modif[0] == 'f')
                full = true;

        _vm_object_print(addr, full, db_printf);
}
#endif

/*
 * 'show' commands
 */
#if 0
extern void	db_show_all_procs(db_expr_t, int, db_expr_t, char*);
extern void	db_show_callout(db_expr_t, int, db_expr_t, char*);
#endif
extern void	db_listbreak_cmd(db_expr_t, int, db_expr_t, char*);
#ifdef OPTION_OPTION_DDB_WATCH
extern void	db_listwatch_cmd(db_expr_t, int, db_expr_t, char*);
#endif
extern void	db_show_regs(db_expr_t, int, db_expr_t, char*);
void		db_show_help(db_expr_t, int, db_expr_t, char*);
#if 0
extern void	db_rsrv_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_rsrvchain_print_cmd(db_expr_t, int, db_expr_t, char*);
#endif
extern void	db_ctxt_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_ctxt_kr_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_ctxt_keys_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_activity_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_inv_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_entry_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_exit_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_invokee_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_invokee_kr_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_invokee_keys_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_irq_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_pins_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_pmem_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_pte_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_mappings_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_uactivity_cmd(db_expr_t, int, db_expr_t, char*);
#if 0
extern void	db_show_reserves_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_kreserves_cmd(db_expr_t, int, db_expr_t, char*);
#endif
extern void	db_show_pages_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_nodes_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_counters_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_key_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_node_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_obhdr_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_sources(db_expr_t, int, db_expr_t, char*);
extern void	db_show_savearea_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_sizes_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_show_gdt(db_expr_t, int, db_expr_t, char*);
extern void	db_show_walkinfo_cmd(db_expr_t, int, db_expr_t, char*);

struct db_command db_show_all_cmds[] = {
#if 0
	{ "procs",	db_show_all_procs,0,	0 },
	{ "callout",	db_show_callout,0,	0 },
#endif
	{ (char *)0 }
};

struct db_command db_show_cmds[] = {
	{ "all",	0,			0,	db_show_all_cmds },
	{ "breaks",	db_listbreak_cmd, 	0,	0 },
	{ "cckr",       db_ctxt_kr_print_cmd,	0,	0 },
	/* 	{ "count",	db_show_counters_cmd,	0,	0 }, */
	{ "entry",      db_entry_print_cmd,	0,	0 },
	{ "exit",       db_exit_print_cmd,	0,	0 },
#ifdef EROS_TARGET_i486
	{ "gdt",	db_show_gdt,	 	0,	0 },
#endif
	{ "inv",        db_inv_print_cmd,	0,	0 },
	{ "invkeys",    db_invokee_keys_print_cmd,0,	0 },
	{ "invokee",    db_invokee_print_cmd,	0,	0 },
	{ "irq",        db_show_irq_cmd,	0,	0 },
	{ "key",        db_show_key_cmd,	0,	0 },
	{ "keyregs",    db_ctxt_keys_print_cmd,	0,	0 },
	{ "keyring",    db_ctxt_kr_print_cmd,	0,	0 },
#if 0
	{ "krsrvs",     db_show_kreserves_cmd,	0,	0 },
#endif
	{ "mappings",   db_show_mappings_cmd,	CS_OWN,	0 },
	{ "node",       db_show_node_cmd,	0,	0 },
	{ "nodes",      db_show_nodes_cmd,	0,	0 },
	{ "obhdr",      db_show_obhdr_cmd,	0,	0 },
	{ "pages",      db_show_pages_cmd,	0,	0 },
	{ "pins",       db_show_pins_cmd,	0,	0 },
	{ "pmem",       db_show_pmem_cmd,	0,	0 },
	{ "proc",       db_ctxt_print_cmd,	0,	0 },
	{ "pte",        db_show_pte_cmd,	0,	0 },
	{ "regs",	db_show_regs,		0,	0 },
#if 0
	{ "rsrvs",      db_show_reserves_cmd,	0,	0 },
	{ "rsrv",       db_rsrv_print_cmd,	0,	0 },
	{ "rsrvchain",  db_rsrvchain_print_cmd,	0,	0 },
#endif
	{ "sa",         db_show_savearea_cmd,	0,	0 },
	{ "sizes",      db_show_sizes_cmd,	0,	0 },
	{ "sources",    db_show_sources,	0,	0 },
	{ "activity",   db_activity_print_cmd,	0,	0 },
	{ "activities", db_show_uactivity_cmd,	0,	0 },
	{ "walkinfo",	db_show_walkinfo_cmd, 	0,	0 },
#ifdef OPTION_OPTION_DDB_WATCH
	{ "watches",	db_listwatch_cmd, 	0,	0 },
#endif
	{ (char *)0, }
};

#ifdef OPTION_KERN_STATS
extern void	db_kstat_hist_depend_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_kstat_hist_objhash_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_kstat_show_cmd(db_expr_t, int, db_expr_t, char*);
#ifdef FAST_IPC_STATS
extern void	db_kstat_fast_cmd(db_expr_t, int, db_expr_t, char*);
#endif
#ifdef OPTION_KERN_TIMING_STATS
extern void	db_kstat_ipc_cmd(db_expr_t, int, db_expr_t, char*);
#endif
extern void	db_kstat_clear_cmd(db_expr_t, int, db_expr_t, char*);

struct db_command db_kstat_hist_cmds[] = {
	{ "depend",	db_kstat_hist_depend_cmd,	CS_OWN,	0 },
	{ "object",	db_kstat_hist_objhash_cmd,	CS_OWN,	0 },
	{ (char *)0 }
};

struct db_command db_kstat_cmds[] = {
	{ "clear",	db_kstat_clear_cmd,	0,	0 },
#ifdef FAST_IPC_STATS
	{ "fast",	db_kstat_fast_cmd,	0,	0 },
#endif
	{ "hist",	0,			0,	db_kstat_hist_cmds },
#ifdef OPTION_KERN_TIMING_STATS
	{ "ipc",	db_kstat_ipc_cmd,	0,	0 },
#endif
	{ "show",	db_kstat_show_cmd,	0,	0 },
	{ (char *)0, }
};
#endif

#ifdef OPTION_KERN_PROFILE
extern void	db_prof_all_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_prof_top_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_prof_clear_cmd(db_expr_t, int, db_expr_t, char*);

struct db_command db_prof_cmds[] = {
	{ "all",	db_prof_all_cmd,	0,		0 },
	{ "clear",	db_prof_clear_cmd,	0,		0 },
	{ "top",	db_prof_top_cmd,	0,		0 },
	{ (char *)0, }
};
#endif

extern void	db_eros_mesg_allinv_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_gate_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_keyerr_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_keeper_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_procinv_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_proctrap_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_return_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_segwalk_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_show_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_uqueue_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_mesg_uyield_cmd(db_expr_t, int, db_expr_t, char*);

struct db_command db_mesg_cmds[] = {
	{ "allinv",	db_eros_mesg_allinv_cmd,	    0,		0 },
	{ "gate",	db_eros_mesg_gate_cmd,		    0,		0 },
	{ "keyerr",	db_eros_mesg_keyerr_cmd,	    0,		0 },
	{ "keeper",	db_eros_mesg_keeper_cmd,	    0,		0 },
	{ "procinv",	db_eros_mesg_procinv_cmd,		    0,		0 },
	{ "proctrap",	db_eros_mesg_proctrap_cmd,		    0,		0 },
	{ "return",	db_eros_mesg_return_cmd,	    0,		0 },
	{ "segwalk",	db_eros_mesg_segwalk_cmd,	    0,		0 },
	{ "show",	db_eros_mesg_show_cmd,		    0,		0 },
	{ "uqueue",	db_eros_mesg_uqueue_cmd,	    0,		0 },
	{ "uyield",	db_eros_mesg_uyield_cmd,	    0,		0 },
	{ (char *)0, }
};

#ifndef NDEBUG
extern void	db_eros_dbg_inttrap_n_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_dbg_inttrap_y_cmd(db_expr_t, int, db_expr_t, char*);

struct db_command db_debug_inttrap_cmds[] = {
	{ "n",	db_eros_dbg_inttrap_n_cmd,	    0,		0 },
	{ "y",	db_eros_dbg_inttrap_y_cmd,	    0,		0 },
	{ (char *)0, }
};
#endif

#if DBG_WILD_PTR
extern void	db_eros_dbg_wild_n_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_eros_dbg_wild_y_cmd(db_expr_t, int, db_expr_t, char*);

struct db_command db_debug_wild_cmds[] = {
	{ "n",	db_eros_dbg_wild_n_cmd,	    0,		0 },
	{ "y",	db_eros_dbg_wild_y_cmd,	    0,		0 },
	{ (char *)0, }
};
#endif

struct db_command db_debug_cmds[] = {
#if DBG_WILD_PTR
	{ "wild",	0,	    0,		db_debug_wild_cmds },
#endif
#ifndef NDEBUG
	{ "inttrap",	0,	    0,		db_debug_inttrap_cmds },
#endif
	{ (char *)0, }
};

extern void	db_check_ctxt_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_check_pages_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_check_nodes_cmd(db_expr_t, int, db_expr_t, char*);

struct db_command db_check_cmds[] = {
	{ "procs",	db_check_ctxt_cmd,	0,		0 },
	{ "nodes",	db_check_nodes_cmd,	0,		0 },
	{ "pages",	db_check_pages_cmd,     0,		0 },
	{ (char *)0, }
};

extern void	db_print_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_examine_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_set_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_search_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_write_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_delete_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_addrspace_cmd(db_expr_t, int, db_expr_t, char*);
extern void     db_breakpoint_cmd(db_expr_t, int, db_expr_t, char*);
#ifdef OPTION_OPTION_DDB_WATCH
extern void	db_deletewatch_cmd(db_expr_t, int, db_expr_t, char*);
extern void     db_watchpoint_cmd(db_expr_t, int, db_expr_t, char*);
#endif
extern void	db_single_step_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_trace_until_call_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_trace_until_matching_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_continue_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_stack_trace_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_reboot_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_node_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_page_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_pframe_cmd(db_expr_t, int, db_expr_t, char*);
void		db_help_cmd(db_expr_t, int, db_expr_t, char*);
void		db_fncall(db_expr_t, int, db_expr_t, char*);
extern void	db_user_continue_cmd(db_expr_t, int, db_expr_t, char*);
extern void	db_user_single_step_cmd(db_expr_t, int, db_expr_t, char*);

struct db_command db_command_table[] = {
#ifdef DB_MACHINE_COMMANDS
  /* this must be the first entry, if it exists */
	{ "machine",    0,                      0,     		0},
#endif
	{ "addrspace",	db_addrspace_cmd,	0,		0 },
	{ "b",		db_breakpoint_cmd,	0,		0 },
	{ "break",	db_breakpoint_cmd,	0,		0 },
	{ "c",		db_continue_cmd,	0,		0 },
	{ "call",	db_fncall,		CS_OWN,		0 },
#if 0
	{ "callout",	db_show_callout,	0,		0 },
#endif
	{ "check",	0,			0,		db_check_cmds },
	{ "continue",	db_continue_cmd,	0,		0 },
	{ "d",		db_delete_cmd,		0,		0 },
	{ "debug",	0,			0,		db_debug_cmds },
	{ "delete",	db_delete_cmd,		0,		0 },
#ifdef OPTION_OPTION_DDB_WATCH
	{ "dwatch",	db_deletewatch_cmd,	0,		0 },
#endif
	{ "examine",	db_examine_cmd,		CS_SET_DOT, 	0 },
	{ "help",	db_help_cmd,		0,		0 },
#ifdef OPTION_KERN_STATS
	{ "kstat",	0,			0,		db_kstat_cmds },
#endif
	{ "match",	db_trace_until_matching_cmd,0,		0 },
	{ "mesg",	0,			0,		db_mesg_cmds },
	{ "n",		db_trace_until_matching_cmd,0,		0 },
	{ "next",	db_trace_until_matching_cmd,0,		0 },
	{ "node",	db_node_cmd,		CS_OWN,		0 },
	{ "page",	db_page_cmd,		CS_OWN,		0 },
	{ "pframe",	db_pframe_cmd,		CS_OWN,		0 },
	{ "print",	db_print_cmd,		0,		0 },
#ifdef OPTION_KERN_PROFILE
	{ "profile",	0,			0,		db_prof_cmds },
#endif
#if 0
	{ "ps",		db_show_all_procs,	0,		0 },
#endif
	{ "rb",		db_reboot_cmd,		0,		0 },
	{ "reboot",	db_reboot_cmd,		0,		0 },
	{ "s",		db_single_step_cmd,	0,		0 },
	{ "search",	db_search_cmd,		CS_OWN|CS_SET_DOT, 0 },
	{ "set",	db_set_cmd,		CS_OWN,		0 },
	{ "show",	0,			0,		db_show_cmds },
	{ "step",	db_single_step_cmd,	0,		0 },
	{ "trace",	db_stack_trace_cmd,	0,		0 },
	{ "uc",		db_user_continue_cmd,	0,		0 },
	{ "us",		db_user_single_step_cmd,	0,		0 },
	{ "until",	db_trace_until_call_cmd,0,		0 },
	{ "w",		db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
#ifdef OPTION_OPTION_DDB_WATCH
	{ "watch",	db_watchpoint_cmd,	CS_MORE,	0 },
#endif
	{ "write",	db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "x",		db_examine_cmd,		CS_SET_DOT, 	0 },
	{ (char *)0, }
};

#ifdef DB_MACHINE_COMMANDS

/* this function should be called to install the machine dependent
   commands. It should be called before the debugger is enabled  */
void db_machine_commands_install(ptr)
struct db_command *ptr;
{
  db_command_table[0].more = ptr;
  return;
}

#endif

struct db_command	*db_last_command = 0;

void
db_help_cmd(db_expr_t dbex, int it, db_expr_t dbtt, char* mc)
{
	int i = 0;
	struct db_command *cmd = db_command_table;

	while (cmd->name != 0) {
	    db_printf("%-12s", cmd->name);
	    i++;
	    if ((i % 6) == 0)
	      db_printf("\n");
	    db_end_line();
	    cmd++;
	}
}

void
db_command_loop()
{
	jmp_buf		db_jmpbuf;
	jmp_buf		*savejmp = db_recover;
	extern int	db_output_line;

	/*
	 * Initialize 'prev' and 'next' to dot.
	 */
	db_prev = db_dot;
	db_next = db_dot;

	(void) setjmp(*(db_recover = &db_jmpbuf));

cantrun:
	db_cmd_loop_done = 0;
	while (!db_cmd_loop_done) {
		if (db_print_position() != 0)
			db_printf("\n");
		db_output_line = 0;

		db_printf("kdb> ");
		(void) db_read_line();

		db_command(&db_last_command, db_command_table);
	}

	// Restore current address space in case we changed it.
	Process * proc = act_CurContext();
	if (proc && ! mach_LoadAddrSpace(proc))
		goto cantrun;

	db_recover = savejmp;
}

void
db_abort_output(void)
{
  db_flush_lex();
  if (db_recover)
	longjmp(*db_recover, 1);
}

void
db_error(char *s)
{
	if (s)
	    db_printf(s);
	db_abort_output();
}


/*
 * Call random function:
 * !expr(arg,arg,arg)
 */
void
db_fncall(db_expr_t dbx, int it, db_expr_t dbt, char* mc)
{
	db_expr_t	fn_addr;
#define	MAXARGS		11
	db_expr_t	args[MAXARGS];
	int		nargs = 0;
	db_expr_t	retval;
	db_expr_t	(*func)(void *, ...);
	int		t;

	if (!db_expression(&fn_addr)) {
	    db_printf("Bad function\n");
	    db_flush_lex();
	    return;
	}
	func = (db_expr_t (*) (void *, ...)) fn_addr;

	t = db_read_token();
	if (t == tLPAREN) {
	    if (db_expression(&args[0])) {
		nargs++;
		while ((t = db_read_token()) == tCOMMA) {
		    if (nargs == MAXARGS) {
			db_printf("Too many arguments\n");
			db_flush_lex();
			return;
		    }
		    if (!db_expression(&args[nargs])) {
			db_printf("Argument missing\n");
			db_flush_lex();
			return;
		    }
		    nargs++;
		}
		db_unread_token(t);
	    }
	    if (db_read_token() != tRPAREN) {
		db_printf("?\n");
		db_flush_lex();
		return;
	    }
	}
	db_skip_to_eol();

	while (nargs < MAXARGS) {
	    args[nargs++] = 0;
	}

	retval = (*func)((void *)args[0], args[1], args[2], args[3], args[4],
			 args[5], args[6], args[7], args[8], args[9] );
	db_printf("%#n\n", retval);
}
