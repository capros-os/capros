/*	$NetBSD: db_trace.c,v 1.17 1995/10/11 04:19:35 mycroft Exp $	*/

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

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>
#include <kernel/Segment.h>

/*#include <kerninc/util.h>*/

extern bool db_sym_numargs(db_sym_t sym, int *nargp, const char **argnames);

extern int strcmp(const char *c1, const char *c2);

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
  { "es",	  (long *)&ddb_regs.ES,     FCN_NULL, false },
  { "ds",	  (long *)&ddb_regs.DS,     FCN_NULL, false },
  { "edi",	  (long *)&ddb_regs.EDI,    FCN_NULL, true },
  { "esi",	  (long *)&ddb_regs.ESI,    FCN_NULL, true },
  { "ebp",	  (long *)&ddb_regs.EBP,    FCN_NULL, true },
  { "ebx",	  (long *)&ddb_regs.EBX,    FCN_NULL, true },
  { "edx",	  (long *)&ddb_regs.EDX,    FCN_NULL, true },
  { "ecx",	  (long *)&ddb_regs.ECX,    FCN_NULL, true },
  { "eax",	  (long *)&ddb_regs.EAX,    FCN_NULL, true },
  { "eip",	  (long *)&ddb_regs.EIP,    FCN_NULL, true },
  { "cs",	  (long *)&ddb_regs.CS,     FCN_NULL, true },
  { "eflags",     (long *)&ddb_regs.EFLAGS, FCN_NULL, false },
  { "esp",	  (long *)&ddb_regs.ESP,    FCN_NULL, false },
  { "ss",	  (long *)&ddb_regs.SS,     FCN_NULL, false } ,
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Stack trace.
 */

struct i386_frame {
	struct i386_frame	*f_frame;
	int			f_retaddr;
	int			f_arg0;
};

#define	NONE		0
#define	TRAP		1
#define	SYSCALL		2
#define	INTERRUPT	3

db_addr_t	db_trap_symbol_value = 0;
db_addr_t	db_syscall_symbol_value = 0;
db_addr_t	db_kdintr_symbol_value = 0;
bool	db_trace_symbols_found = false;
bool isUser;

void
db_find_trace_symbols()
{
	db_expr_t	value;

	if (db_value_of_name("_trap", &value))
		db_trap_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_kdintr", &value))
		db_kdintr_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_syscall", &value))
		db_syscall_symbol_value = (db_addr_t) value;
	db_trace_symbols_found = true;
}

/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
int
db_numargs(struct i386_frame * fpp/* fp */)
{
#if 0
	int	*argp;
	int	inst;
	int	args;

	argp = (int *)db_get_value((int)&fp->f_retaddr, 4, false);
	if (argp < (int *)VM_MIN_KERNEL_ADDRESS || argp > (int *)&etext) {
		args = 5;
	} else {
		inst = db_get_value((int)argp, 4, false);
		if ((inst & 0xff) == 0x59)	/* popl %ecx */
			args = 1;
		else if ((inst & 0xffff) == 0xc483)	/* addl %n, %esp */
			args = ((inst >> 16) & 0xff) / 4;
		else
			args = 5;
	}
	return (args);
#else
	return 0;
#endif
}

/* 
 * Figure out the next frame up in the call stack.  
 * For trap(), we print the address of the faulting instruction and 
 *   proceed with the calling frame.  We return the ip that faulted.
 *   If the trap was caused by jumping through a bogus pointer, then
 *   the next line in the backtrace will list some random function as 
 *   being called.  It should get the argument list correct, though.  
 *   It might be possible to dig out from the next frame up the name
 *   of the function that faulted, but that could get hairy.
 */
static void
db_nextframe(struct i386_frame **fp, /* in/out */
	     db_addr_t *ip,	/* out */
	     int *argp,		/* in */
	     int is_trap	/* in */
	     )
{
	uva_t offset = isUser ? KUVA : 0;
	switch (is_trap) {
	    case NONE:
		*ip = (db_addr_t)
			db_get_value((int) &(*fp)->f_retaddr + offset, 4, false);
		*fp = (struct i386_frame *)
			db_get_value((int) &(*fp)->f_frame + offset, 4, false);
		break;

	    default: {
		savearea_t *tf; /* trap frame */

		/* The only argument to trap() or syscall() is the trapframe. */
		tf = * ((savearea_t **)argp);/* on EROS, the */
						  /* trapframe ptr */

		*fp = (struct i386_frame *)tf->EBP;
		*ip = (db_addr_t)tf->EIP;
		switch (is_trap) {
		case TRAP:
		  db_printf("--- trap (EIP=0x%08x vector=0x%x sa=0x%08x) ---\n",
			    tf->EIP, tf->ExceptNo, tf);
		  break;
		case SYSCALL:
		  /* All syscalls do a pusha and then use ebp for other than
		     the frame pointer.
		     Get the saved ebp from the pusha frame. */
		  *fp = (struct i386_frame *)
			db_get_value(tf->ESP + 8 + KUVA, 4, false);
		  db_printf("--- IPC (EIP=0x%08x OC=%d sa=0x%08x) ---\n",
			    tf->EIP, tf->EAX, tf);
		  break;
#if 0
		case INTERRUPT:
			db_printf("--- interrupt ---\n");
			break;
#endif
		}
		isUser = (tf->DS == sel_DomainData);
		    
		break;
	    }
	}
}

void
db_stack_trace_cmd(db_expr_t addr, int have_addr,
		   db_expr_t count, char *modif)
{
  struct i386_frame *frame, *lastframe;
#if 0
  int		*argp = 0;
#endif
  db_addr_t	callpc;
  int		is_trap = 0;

#if 0
  if (!db_trace_symbols_found)
    db_find_trace_symbols();
#endif

  {	// scan modifiers
    register char *cp = modif;
    register char c;

    while ((c = *cp++) != 0) {
      // No modifiers are used.
    }
  }

  if (count == -1)
    count = 65535;

  if (!have_addr) {
    frame = (struct i386_frame *)ddb_regs.EBP;
    callpc = (db_addr_t)ddb_regs.EIP;
    isUser = false;
  } else {
    frame = (struct i386_frame *)addr;
    callpc = (db_addr_t)
      db_get_value((int)&frame->f_retaddr, 4, false);
    isUser = true;
  }

  lastframe = 0;
  while (count && frame != 0) {
    int		narg;
    const char *	name;
    db_expr_t	offset;
    db_sym_t	sym;
#define MAXNARG	16
    const char	*argnames[MAXNARG], **argnp = NULL;

    sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
    db_symbol_values(sym, &name, NULL);

    if (lastframe == 0 && sym == NULL) {
      /* Symbol not found, peek at code */
      int	instr = db_get_value(callpc, 4, false);

      offset = 1;
      if ((instr & 0x00ffffff) == 0x00e58955 ||
	  /* enter: pushl %ebp, movl %esp, %ebp */
	  (instr & 0x0000ffff) == 0x0000e589
	  /* enter+1: movl %esp, %ebp */) {
	offset = 0;
      }
    }

    if (name) {
#if 0
      db_printf("%s\n", name);
#endif
      if (!strcmp(name,
		  "idt_OnTrapOrInterrupt")) {
	is_trap = TRAP;
      }
      else if (!strcmp(name,
		       "idt_OnKeyInvocationTrap")) {
	is_trap = SYSCALL;
      }
      else if (!strcmp(name,
		       "LowYield")) {
	db_printf("--- Yield ---\n");
	return;
      }
#if 0
      else if (!strcmp(name, "intr_clock")) {
	is_trap = TRAP;
      }
      else if (!strcmp(name, "intr_InvokeKey")) {
	is_trap = TRAP;
      }
      else if (!strcmp(name, "int_CapInstr")) {
	is_trap = TRAP;
      }
      if (!strcmp(name, "_trap")) {
	is_trap = TRAP;
      } else if (!strcmp(name, "_syscall")) {
	is_trap = SYSCALL;
      } else if (name[0] == '_' && name[1] == 'X') {
	if (!strncmp(name, "_Xintr", 6) ||
	    !strncmp(name, "_Xresume", 8) ||
	    !strncmp(name, "_Xstray", 7) ||
	    !strncmp(name, "_Xhold", 6) ||
	    !strncmp(name, "_Xrecurse", 9) ||
	    !strcmp(name, "_Xdoreti") ||
	    !strncmp(name, "_Xsoft", 6)) {
	  is_trap = INTERRUPT;
	} else
	  goto normal;
      }
#endif
      else
	goto normal;
      narg = 0;
    } else {
    normal:
      is_trap = NONE;
      narg = MAXNARG;
      if (db_sym_numargs(sym, &narg, argnames))
	argnp = argnames;
      else
	narg = db_numargs(frame);
    }

#if 0
    db_printf("%s(", name);

    if (lastframe == 0 && offset == 0 && !have_addr) {
      /*
       * We have a breakpoint before the frame is set up
       * Use %esp instead
       */
      argp = &((struct i386_frame *)(ddb_regs.ESP-4))->f_arg0;
    } else {
      argp = &frame->f_arg0;
    }

    while (narg) {
      if (argnp)
	db_printf("%s=", *argnp++);
      db_printf("%x", db_get_value((int)argp, 4, false));
      argp++;
      if (--narg != 0)
	db_printf(",");
    }
    db_printf(") at ");
#endif
    db_printf("0x%08x: [FP=0x%08x] ", callpc, frame);
    db_printsym(callpc, DB_STGY_PROC);
    db_printf("\n");

    if (lastframe == 0 && offset == 0 && !have_addr) {
      /* Frame really belongs to next callpc */
      lastframe = (struct i386_frame *)(ddb_regs.ESP-4);
      callpc = (db_addr_t)
	db_get_value((int)&lastframe->f_retaddr, 4, false);
#if 0
      db_printf("Next frame call pc: 0x%08x is_trap=%d\n",
		callpc, is_trap);
#endif
      continue;
    }

    lastframe = frame;
    db_nextframe(&frame, &callpc, &frame->f_arg0, is_trap);

#if 0
    db_printf("db_nextframe() => callpc 0x%08x frame 0x%08x"
	      " is_trap %d\n",
	      callpc, frame, is_trap);
#endif

    if (frame == 0) {
      /* end of chain */
      break;
    }

    --count;
  }
#if 0
  if (count && is_trap != NONE) {
    db_printsym(callpc, DB_STGY_XTRN);
    db_printf(":\n");
  }
#endif
}
