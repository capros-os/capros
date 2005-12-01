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

#include <arch-kerninc/db_machdep.h>
#include <arch-kerninc/setjmp.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_access.h>
#include <kerninc/IRQ.h>
/* #include <kerninc/Console.hxx> */
#include <kerninc/KernStream.h>

#define cnpollc(x) kstream_dbg_stream->SetDebugging((x))

void kdbprinttrap(int type, int code);
extern void db_trap(int type, int code);

extern jmp_buf	*db_recover;

int	db_active = 0;
db_regs_t ddb_regs;

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(int type, int code, register db_regs_t *regs)
{
#if 0
	int s;
#endif

#if 0
	if ((boothowto&RB_KDB) == 0)
		return(0);
#endif

	switch (type) {
	case T_BPTFLT:	/* breakpoint */
	case T_TRCTRAP:	/* single_step */
	case -1:	/* keyboard interrupt */
		break;
	default:
		kdbprinttrap(type, code);
		if (db_recover != 0) {
			db_error("Faulted in OPTION_DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* XXX Should switch to kdb`s own stack here. */

	/* We REALLY got a kernel save area. */
	ddb_regs.EDI    = regs->EDI;
	ddb_regs.ESI    = regs->ESI;
	ddb_regs.EBP    = regs->EBP;
	ddb_regs.EBX    = regs->EBX;
	ddb_regs.EDX    = regs->EDX;
	ddb_regs.ECX    = regs->ECX;
	ddb_regs.EAX    = regs->EAX;
	ddb_regs.EIP    = regs->EIP;
	ddb_regs.CS     = regs->CS & 0xffffu;
	ddb_regs.EFLAGS = regs->EFLAGS;

	if (KERNELMODE(regs->CS, regs->EFLAGS)) {
		/*
		 * Kernel mode - esp and ss not saved
		 */
		ddb_regs.ESP = (int)&regs->ESP;	/* kernel stack pointer */

		asm("movw %%ss,%w0" : "=r" (ddb_regs.SS));
		asm("movw %%ds,%w0" : "=r" (ddb_regs.DS));
		asm("movw %%es,%w0" : "=r" (ddb_regs.ES));
		asm("movw %%fs,%w0" : "=r" (ddb_regs.FS));
		asm("movw %%gs,%w0" : "=r" (ddb_regs.GS));

		ddb_regs.SS &= 0xffffu;
		ddb_regs.DS &= 0xffffu;
		ddb_regs.ES &= 0xffffu;
		ddb_regs.FS &= 0xffffu;
		ddb_regs.GS &= 0xffffu;
	}
	else {
	  ddb_regs.DS  = regs->DS & 0xffffu;
	  ddb_regs.ES  = regs->ES & 0xffffu;
	  ddb_regs.FS  = regs->FS & 0xffffu;
	  ddb_regs.GS  = regs->GS & 0xffffu;

	  ddb_regs.SS  = regs->SS & 0xffffu;
	  ddb_regs.ESP  = regs->ESP;
	}

#ifdef DB_DEBUG
	MsgLog::printf("Enter w/ regs 0x%08x pc 0x%08x, flags 0x%08x esp=0x%08x\n",
		       regs, regs->EIP, regs->EFLAGS, regs->ESP);
#endif


	irq_DISABLE();
	db_active++;
	cnpollc(true);
	db_trap(type, code);
	cnpollc(false);
	db_active--;
	irq_ENABLE();


	regs->EDI    = ddb_regs.EDI;
	regs->ESI    = ddb_regs.ESI;
	regs->EBP    = ddb_regs.EBP;
	regs->EBX    = ddb_regs.EBX;
	regs->EDX    = ddb_regs.EDX;
	regs->ECX    = ddb_regs.ECX;
	regs->EAX    = ddb_regs.EAX;
	regs->EIP    = ddb_regs.EIP;
	regs->CS     = ddb_regs.CS;
	regs->EFLAGS = ddb_regs.EFLAGS;

	if (!KERNELMODE(regs->CS, regs->EFLAGS)) {
		/* ring transit - saved esp and ss valid */
	  regs->DS     = ddb_regs.DS;
	  regs->ES     = ddb_regs.ES;
	  regs->FS     = ddb_regs.FS;
	  regs->GS     = ddb_regs.GS;

	  regs->ESP    = ddb_regs.ESP;
	  regs->SS     = ddb_regs.SS;
	}

#ifdef DB_DEBUG
	MsgLog::printf("Resume w/ regs 0x%08x pc 0x%08x, flags 0x%08x, esp=0x%08x\n",
		       regs, regs->EIP, regs->EFLAGS, regs->ESP);
#endif
	return (1);
}

/* For now... */
char *trap_type[0];
int trap_types = 0;

/*
 * Print trap reason.
 */
void
kdbprinttrap(int type, int code)
{
	db_printf("kernel: ");
	if (type >= trap_types || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", trap_type[type]);
	db_printf(" trap, code=%x\n", code);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(db_addr_t addr, register int size, register char *data)
{
	register char	*src;

	src = (char *)addr;
	while (--size >= 0)
		*data++ = *src++;
}

#if 0
pt_entry_t *pmap_pte __P((pmap_t, vm_offset_t));
#endif

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(db_addr_t addr, register int size,
			   register char * data) 
{
#if 0
	register char	*dst;

	register pt_entry_t *ptep0 = 0;
	pt_entry_t	oldmap0 = { 0 };
	vm_offset_t	addr1;
	register pt_entry_t *ptep1 = 0;
	pt_entry_t	oldmap1 = { 0 };
	extern char	etext;

	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr < (vm_offset_t)&etext) {
		ptep0 = pmap_pte(pmap_kernel(), addr);
		oldmap0 = *ptep0;
		*(int *)ptep0 |= /* INTEL_PTE_WRITE */ PG_RW;

		addr1 = i386_trunc_page(addr + size - 1);
		if (i386_trunc_page(addr) != addr1) {
			/* data crosses a page boundary */
			ptep1 = pmap_pte(pmap_kernel(), addr1);
			oldmap1 = *ptep1;
			*(int *)ptep1 |= /* INTEL_PTE_WRITE */ PG_RW;
		}
		pmap_update();
	}

	dst = (char *)addr;

	while (--size >= 0)
		*dst++ = *data++;

	if (ptep0) {
		*ptep0 = oldmap0;
		if (ptep1)
			*ptep1 = oldmap1;
		pmap_update();
	}
#else
	register char	*dest;

	dest = (char *)addr;
	while (--size >= 0)
		*dest++ = *data++;
#endif
}

void
Debugger()
{
	asm("int $3");
}
