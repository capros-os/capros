/*	$NetBSD: db_interface.c,v 1.18 1995/10/10 04:45:03 mycroft Exp $	*/

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
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/* #define DB_DEBUG */

/*
 * Interface to new debugger.
 */

#include <string.h>
#include <arch-kerninc/db_machdep.h>
#include <arch-kerninc/setjmp.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <kerninc/KernStream.h>
#include <kerninc/Process.h>
#include <arch-kerninc/IRQ-inline.h>

#define cnpollc(x) kstream_dbg_stream->SetDebugging((x))

extern void db_trap(int type, int code);

int	db_active = 0;
db_regs_t ddb_regs;

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
  { "r0",   (long *)&ddb_regs.r0,   FCN_NULL, true },
  { "r1",   (long *)&ddb_regs.r1,   FCN_NULL, true },
  { "r2",   (long *)&ddb_regs.r2,   FCN_NULL, true },
  { "r3",   (long *)&ddb_regs.r3,   FCN_NULL, true },
  { "r4",   (long *)&ddb_regs.r4,   FCN_NULL, true },
  { "r5",   (long *)&ddb_regs.r5,   FCN_NULL, true },
  { "r6",   (long *)&ddb_regs.r6,   FCN_NULL, true },
  { "r7",   (long *)&ddb_regs.r7,   FCN_NULL, true },
  { "r8",   (long *)&ddb_regs.r8,   FCN_NULL, true },
  { "r9",   (long *)&ddb_regs.r9,   FCN_NULL, true },
  { "r10",  (long *)&ddb_regs.r10,  FCN_NULL, true },
  { "r11",  (long *)&ddb_regs.r11,  FCN_NULL, true },
  { "r12",  (long *)&ddb_regs.r12,  FCN_NULL, true },
  { "sp",   (long *)&ddb_regs.r13,  FCN_NULL, true },
  { "r14",  (long *)&ddb_regs.r14,  FCN_NULL, true },
  { "pc",   (long *)&ddb_regs.r15,  FCN_NULL, true },
  { "cpsr", (long *)&ddb_regs.CPSR, FCN_NULL, false },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
void
kdb_trap(int type,	// always T_BPTFLT
         int code, register db_regs_t *regs)
{
	/* XXX Should switch to kdb`s own stack here. */

	memcpy(&ddb_regs, regs, sizeof(db_regs_t));

#ifdef DB_DEBUG
	MsgLog::printf("Enter w/ regs 0x%08x pc 0x%08x, cpsr 0x%08x sp=0x%08x\n",
		       regs, regs->r15, regs->CPSR, regs->r13);
#endif

	irq_DISABLE();
	db_active++;
	cnpollc(true);
	db_trap(type, code);
	cnpollc(false);
	db_active--;
	irq_ENABLE();

	memcpy(regs, &ddb_regs, sizeof(db_regs_t));

#ifdef DB_DEBUG
	MsgLog::printf("Resume w/ regs 0x%08x pc 0x%08x, cpsr 0x%08x, sp=0x%08x\n",
		       regs, regs->r15, regs->CPSR, regs->r13);
#endif
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(db_addr_t addr, register int size, register char *data)
{
	register char * src = (char *)addr;
	while (--size >= 0)
		*data++ = *src++;
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(db_addr_t addr, register int size,
			   register char * data) 
{
	register char * dest = (char *)addr;
	while (--size >= 0)
		*dest++ = *data++;
}

unsigned int
getreg_val(int r)
{
  return 0;	// this procedure is unused
}

unsigned int
branch_taken(unsigned int inst, db_addr_t pc,
             unsigned int (*getreg_val)(void), db_regs_t * regs)
{
  printf("Single stepping not supported!\n");
  return pc;
}

/* Print the machine-dependent part of the Process structure. */
void
db_eros_print_context_md(Process * cc)
{
  db_printf(" flmtProd=0x%08x flmt=0x%08x pid=0x%08x dacr=0x%08x\n",
            cc->md.flmtProducer, cc->md.firstLevelMappingTable,
            cc->md.pid, cc->md.dacr);
}

void
db_stack_trace_cmd(db_expr_t addr, int have_addr,
		   db_expr_t count, char *modif)
{
  db_printf("Stack trace not implemented.\n");
}

/*
 * Disassemble instruction at 'loc'.  'altfmt' specifies an
 * (optional) alternate format.  Return address of start of
 * next instruction.
 */
db_addr_t
db_disasm(db_addr_t loc, bool altfmt)
{
  // not implemented yet
  return loc+4;
}
