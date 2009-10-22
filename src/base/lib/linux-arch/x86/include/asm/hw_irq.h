#ifndef _ASM_X86_HW_IRQ_H
#define _ASM_X86_HW_IRQ_H

/*
 * (C) 1992, 1993 Linus Torvalds, (C) 1997 Ingo Molnar
 *
 * moved some of the old arch/i386/kernel/irq.h to here. VY
 *
 * IRQ/IPI changes taken from work by Thomas Radke
 * <tomsoft@informatik.tu-chemnitz.de>
 *
 * hacked by Andi Kleen for x86-64.
 * unified by tglx
 */

#include <asm/irq_vectors.h>

#ifndef __ASSEMBLY__

#include <linux/percpu.h>
#include <linux/profile.h>
#include <linux/smp.h>

#include <asm/atomic.h>
#include <asm/irq.h>
#include <asm/sections.h>

/* Interrupt handlers registered during init_IRQ */
extern void apic_timer_interrupt(void);
extern void generic_interrupt(void);
extern void error_interrupt(void);
extern void spurious_interrupt(void);
extern void thermal_interrupt(void);
extern void reschedule_interrupt(void);

extern void invalidate_interrupt(void);
extern void invalidate_interrupt0(void);
extern void invalidate_interrupt1(void);
extern void invalidate_interrupt2(void);
extern void invalidate_interrupt3(void);
extern void invalidate_interrupt4(void);
extern void invalidate_interrupt5(void);
extern void invalidate_interrupt6(void);
extern void invalidate_interrupt7(void);

extern void irq_move_cleanup_interrupt(void);
extern void threshold_interrupt(void);

extern void call_function_interrupt(void);
extern void call_function_single_interrupt(void);

/* PIC specific functions */
extern void disable_8259A_irq(unsigned int irq);
extern void enable_8259A_irq(unsigned int irq);
extern int i8259A_irq_pending(unsigned int irq);
extern void make_8259A_irq(unsigned int irq);
extern void init_8259A(int aeoi);

/* IOAPIC */
#define IO_APIC_IRQ(x) (((x) >= NR_IRQS_LEGACY) || ((1<<(x)) & io_apic_irqs))
extern unsigned long io_apic_irqs;

extern void init_VISWS_APIC_irqs(void);
extern void setup_IO_APIC(void);
extern void disable_IO_APIC(void);
extern int IO_APIC_get_PCI_irq_vector(int bus, int slot, int fn);
extern void setup_ioapic_dest(void);

extern void enable_IO_APIC(void);

/* Statistics */
extern atomic_t irq_err_count;
extern atomic_t irq_mis_count;

/* EISA */
extern void eisa_set_level_irq(unsigned int irq);

/* SMP */
extern void smp_apic_timer_interrupt(struct pt_regs *);
extern void smp_spurious_interrupt(struct pt_regs *);
extern void smp_error_interrupt(struct pt_regs *);
#ifdef CONFIG_SMP
extern void smp_reschedule_interrupt(struct pt_regs *);
extern void smp_call_function_interrupt(struct pt_regs *);
extern void smp_call_function_single_interrupt(struct pt_regs *);
#ifdef CONFIG_X86_32
extern void smp_invalidate_interrupt(struct pt_regs *);
#else
extern asmlinkage void smp_invalidate_interrupt(struct pt_regs *);
#endif
#endif

extern void (*__initconst interrupt[NR_VECTORS-FIRST_EXTERNAL_VECTOR])(void);

typedef int vector_irq_t[NR_VECTORS];
DECLARE_PER_CPU(vector_irq_t, vector_irq);

#ifdef CONFIG_X86_IO_APIC
extern void lock_vector_lock(void);
extern void unlock_vector_lock(void);
extern void __setup_vector_irq(int cpu);
#else
static inline void lock_vector_lock(void) {}
static inline void unlock_vector_lock(void) {}
static inline void __setup_vector_irq(int cpu) {}
#endif

#endif /* !ASSEMBLY_ */

#endif /* _ASM_X86_HW_IRQ_H */
