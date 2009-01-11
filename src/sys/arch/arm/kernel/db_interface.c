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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

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
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <kerninc/Activity.h>
#include <kerninc/KernStats.h>
#include <kerninc/KernStream.h>
#include <kerninc/Process.h>
#include <kerninc/IRQ.h>
#include <arch-kerninc/PTE.h>

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

	irqFlags_t flags = local_irq_save();
	db_active++;
	kstream_dbg_stream->SetDebugging(true);
	db_trap(type, code);
	kstream_dbg_stream->SetDebugging(false);
	db_active--;
	local_irq_restore(flags);

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
  uint8_t * src = (uint8_t *)addr;
  while (--size >= 0) {
    if (! SafeLoadByte(src++, (uint8_t *)data)) {
      // Couldn't load. Fill the rest with zero.
      printf("Read failure\n");	// no other way to signal this
      memset(data, 0, size+1);
      break;
    }
    data++;
  }
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(db_addr_t addr, register int size,
		  register char * data) 
{
  uint8_t * dest = (uint8_t *)addr;
  while (--size >= 0) {
    if (! SafeStoreByte(dest++, *data++)) {
      printf("Write failure\n");	// no other way to signal this
      break;	// couldn't store
    }
  }
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

typedef uint32_t FramePtr;

db_expr_t
FramePtr_GetRetAddr(FramePtr fp)
{
  return db_get_value((int)(fp-4), 4, false);
}

db_expr_t
FramePtr_GetNextFrame(FramePtr fp)
{
  return db_get_value((int)(fp-12), 4, false);
}

void
db_stack_trace_cmd(db_expr_t addr, int have_addr,
		   db_expr_t count, char *modif)
{
  bool	kernel_only = true;
  bool	trace_thread = false;

  {	// scan modifiers
    register char *cp = modif;
    register char c;

    while ((c = *cp++) != 0) {
      if (c == 't')
	trace_thread = true;
      if (c == 'u')
	kernel_only = false;
    }
  }

  if (count == -1)
    count = 65535;

  db_addr_t callpc = 0;
  FramePtr frame = 0;

  if (!have_addr) {
    frame = (FramePtr)ddb_regs.r11;	// fp register
    callpc = (db_addr_t)ddb_regs.r15;	// pc register
  } else if (trace_thread) {
    db_printf ("db_interface.c: can't trace thread\n");
  } else {
    frame = (FramePtr)addr;
    callpc = (db_addr_t) FramePtr_GetRetAddr(frame);
  }
  FramePtr prevFrame = 0;

  while (count && frame != 0) {
    const char * name;
    db_expr_t	offset;
    db_sym_t	sym;

    sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
    db_symbol_values(sym, &name, NULL);

#if 0
    if (name) {
      db_printf("%s\n", name);
    }
#endif

    if (frame <= 7) {
      // Values of the frame pointer from 1 through 7 are used to signal an
      // exception.
      FramePtr thisFrame = frame;
      callpc = frame = 0;	// by default, this is the end of the line
      switch (thisFrame) {
        case 1: printf("[Undefined instr exception]"); goto tryCurrent;
        case 2: printf("[SWI exception]"); goto tryCurrent;
        case 3: printf("[Prefetch abort exception]"); goto tryCurrent;
        case 4: printf("[Data abort exception]"); goto tryCurrent;
tryCurrent:
          if (act_Current()) {
            Process * p = proc_Current();
            if (p) {
              frame = p->trapFrame.r11;
              callpc = p->trapFrame.r15;
            }
          }
          break;

        case 5: break;	// no such exception - must be corruption
        case 6: printf("[IRQ exception]");
        {
          // This is very dependent on the exception handling code.
          db_addr_t irqsp = db_get_value((int)(prevFrame+4), 4, false);
          frame = db_get_value((int)(irqsp+0), 4, false);
          callpc = db_get_value((int)(irqsp+8), 4, false);
          break;
        }
        case 7: printf("[FIQ exception]"); break;
      }
      prevFrame = thisFrame;
    } else {
      db_printf("0x%08x: ", callpc);
      db_printf("[FP=0x%08x] ", frame);
      db_printsym(callpc, DB_STGY_PROC);
      prevFrame = frame;
      callpc = (db_addr_t) FramePtr_GetRetAddr(frame);
      frame = (FramePtr) FramePtr_GetNextFrame(frame);
    }
    db_printf("\n");

    if (frame == 0) {
      /* end of chain */
      break;
    }

    --count;
  }
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

#define DB64(x) ((uint32_t)((x)>>32)), (uint32_t)(x)
void
KernStats_PrintMD(void)
{
  db_printf("nUncache  %6llu  "
            "nYldMaps  %6llu  "
            "nDomSteal %6llu  "
            "nSSSteal  %6llu\n",
            KernStats.nPageUncache,
            KernStats.nYieldForMaps,
            KernStats.nDomainSteal,
            KernStats.nSmallSpaceSteal
            );
}
