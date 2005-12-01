/*	$NetBSD: db_machdep.h,v 1.8 1994/10/27 04:16:02 cgd Exp $	*/

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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef	_I386_DB_MACHDEP_H_
#define	_I386_DB_MACHDEP_H_

/*
 * Machine-dependent defines for new kernel debugger.
 */

#include <kerninc/kernel.h>
#include <kerninc/Process.h>
#include <kerninc/memory.h>

INLINE int strncmp(const char *c1, const char *c2, uint32_t len)
{
  return memcmp(c1, c2, len);
}

#define kernel_map  KernPageDir_pa
#define PSL_T	    0x100	/* trace trap bit */
#define PSL_VM	    0x20000	/* VM86 bit */
#define T_BPTFLT    3
#define T_TRCTRAP   1

#define SEL_KPL   0		/* kernel privilege level */
#define SEL_UPL   3		/* user privilege level */
#define SEL_RPL   3		/* requestor's privilege level mask */
#define ISPL(c) ((c) & SEL_RPL)
#define KERNELMODE(c, f) ((ISPL(c) == SEL_KPL) && (((f) & PSL_VM) == 0))

/* #define SOFTWARE_SSTEP */

/* db_addr_t should be typedef'ed to kva_t, but that causes
   too many type mismatches. */
typedef	unsigned long	db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */
typedef uint32_t	vm_map_t;	/* mapping table id */
typedef kva_t		vm_offset_t;	/* VM offset */
typedef size_t		vm_size_t;	/* VM size */

typedef struct savearea db_regs_t;

extern db_regs_t	ddb_regs;	/* register state */
#define	OPTION_DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((regs)->EIP)

#define	BKPT_INST	0xcc		/* breakpoint instruction */
#define	BKPT_SIZE	(1)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#if 0
#define	FIXUP_PC_AFTER_BREAK		ddb_regs.EIP -= BKPT_SIZE;
#endif

#define	db_clear_single_step(regs)	((regs)->EFLAGS &= ~PSL_T)
#define	db_set_single_step(regs)	((regs)->EFLAGS |=  PSL_T)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BPTFLT)
#define IS_WATCHPOINT_TRAP(type, code)	((type) == T_TRCTRAP && (code) & 15)

#define	I_CALL		0xe8
#define	I_CALLI		0xff
#define	I_RET		0xc3
#define	I_IRET		0xcf

#define	inst_trap_return(ins)	(((ins)&0xff) == I_IRET)
#define	inst_return(ins)	(((ins)&0xff) == I_RET)
#define	inst_call(ins)		(((ins)&0xff) == I_CALL || \
				 (((ins)&0xff) == I_CALLI && \
				  ((ins)&0x3800) == 0x1000))
#define inst_load(ins)		0
#define inst_store(ins)		0

/* access capability and access macros */

#define DB_ACCESS_LEVEL		2	/* access any space */
#define DB_CHECK_ACCESS(addr,size,task)				\
	db_check_access(addr,size,task)
#define DB_PHYS_EQ(task1,addr1,task2,addr2)			\
	db_phys_eq(task1,addr1,task2,addr2)
#define DB_VALID_KERN_ADDR(addr)				\
	((addr) >= VM_MIN_KERNEL_ADDRESS && 			\
	 (addr) < VM_MAX_KERNEL_ADDRESS)
#define DB_VALID_ADDRESS(addr,user)				\
	((!(user) && DB_VALID_KERN_ADDR(addr)) ||		\
	 ((user) && (addr) < VM_MAX_ADDRESS))

bool db_check_access(/* vm_offset_t, int, task_t */);
bool db_phys_eq(/* task_t, vm_offset_t, task_t, vm_offset_t */);

/* macros for printing OS server dependent task name */

#define DB_TASK_NAME(task)	db_task_name(task)
#define DB_TASK_NAME_TITLE	"COMMAND                "
#define DB_TASK_NAME_LEN	23
#define DB_NULL_TASK_NAME	"?                      "

void		db_task_name(/* task_t */);

/* macro for checking if a thread has used floating-point */

#define db_thread_fp_used(thread)	((thread)->pcb->ims.ifps != 0)

#endif	/* _I386_DB_MACHDEP_H_ */
