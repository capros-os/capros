/*
 *	linux/kernel/softirq.c
 *
 *	Copyright (C) 1992 Linus Torvalds
 *
 * Rewritten. Old one was good in 2.2, but in 2.3 it was immoral. --ANK (990903)
 */
/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/kthread.h>
#include <linux/rcupdate.h>
#include <linux/smp.h>

#include <asm/irq.h>

#ifndef __ARCH_IRQ_STAT
irq_cpustat_t irq_stat[NR_CPUS] ____cacheline_aligned;
EXPORT_SYMBOL(irq_stat);
#endif

static struct softirq_action softirq_vec[32] __cacheline_aligned_in_smp;

static DEFINE_PER_CPU(struct task_struct *, ksoftirqd);
static DEFINE_PER_CPU(bool, ksoftirqd_active);
static DEFINE_PER_CPU(struct semaphore, ksoftirqd_sem);

asmlinkage void __do_softirq(void)
{
	struct softirq_action *h;
	__u32 pending;

	trace_softirq_enter();
	pending = local_softirq_pending();
	do {
		/* Reset the pending bitmask before enabling irqs */
		set_softirq_pending(0);

		local_irq_enable();
	
		h = softirq_vec;
		do {
			if (pending & 1) {
				h->action(h);
			}
			h++;
			pending >>= 1;
		} while (pending);

		local_irq_disable();

		pending = local_softirq_pending();
	} while (pending);

	trace_softirq_exit();
}

#ifndef __ARCH_HAS_DO_SOFTIRQ

asmlinkage void do_softirq(void)
{
	__u32 pending;
	unsigned long flags;

	local_irq_save(flags);

	pending = local_softirq_pending();

	if (pending)
		__do_softirq();

	local_irq_restore(flags);
}

EXPORT_SYMBOL(do_softirq);

#endif

void __raise_softirq_irqoff(unsigned int nr)
{
	or_softirq_pending(1UL << (nr));
	// Ensure ksoftirqd sees the pending softirq:
	if (! per_cpu(ksoftirqd_active, smp_processor_id()))
		up(& per_cpu(ksoftirqd_sem, smp_processor_id()));
}

void fastcall raise_softirq(unsigned int nr)
{
	unsigned long flags;

	local_irq_save(flags);
	raise_softirq_irqoff(nr);
	local_irq_restore(flags);
}

static int ksoftirqd(void * __bind_cpu)
{
//	set_user_nice(current, 19);

	while (!kthread_should_stop()) {
		down(& per_cpu(ksoftirqd_sem, smp_processor_id()));

		while (local_softirq_pending()) {
			/* ksoftirqd_active == true means we are going to
			recheck for pending softirq's before sleeping,
			so raise_softirq() needn't up(per_cpu(ksoftirqd_sem)).
			*/
			per_cpu(ksoftirqd_active, smp_processor_id()) = true;
			do_softirq();
			per_cpu(ksoftirqd_active, smp_processor_id()) = false;
		}
	}
	return 0;
}

static void ensure_ksoftirqd(void)
{
#ifdef CONFIG_SMP
#error FIXME
#else
	int hotcpu = 0;
	if (!per_cpu(ksoftirqd, hotcpu)) {
		sema_init(& per_cpu(ksoftirqd_sem, hotcpu), 0);
		struct task_struct * p;
		p = kthread_run(ksoftirqd, (void *)hotcpu, "ksoftirqd");
		if (IS_ERR(p)) {
			printk("ksoftirqd for %i failed\n", hotcpu);
			return;
		}
  		per_cpu(ksoftirqd, hotcpu) = p;
	}
#endif
}

void open_softirq(int nr, void (*action)(struct softirq_action*), void *data)
{
	softirq_vec[nr].data = data;
	softirq_vec[nr].action = action;
	ensure_ksoftirqd();
}

/* Tasklets */
struct tasklet_head
{
	struct tasklet_struct *list;
};

/* Some compilers disobey section attribute on statics when not
   initialized -- RR */
static DEFINE_PER_CPU(struct tasklet_head, tasklet_vec) = { NULL };
static DEFINE_PER_CPU(struct tasklet_head, tasklet_hi_vec) = { NULL };

static void tasklet_action(struct softirq_action *a)
{
	struct tasklet_struct *list;

	local_irq_disable();
	list = __get_cpu_var(tasklet_vec).list;
	__get_cpu_var(tasklet_vec).list = NULL;
	local_irq_enable();

	while (list) {
		struct tasklet_struct *t = list;

		list = list->next;

		if (tasklet_trylock(t)) {
			if (!atomic_read(&t->count)) {
				if (!test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
					BUG();
				t->func(t->data);
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}

		local_irq_disable();
		t->next = __get_cpu_var(tasklet_vec).list;
		__get_cpu_var(tasklet_vec).list = t;
		__raise_softirq_irqoff(TASKLET_SOFTIRQ);
		local_irq_enable();
	}
}

static void tasklet_hi_action(struct softirq_action *a)
{
	struct tasklet_struct *list;

	local_irq_disable();
	list = __get_cpu_var(tasklet_hi_vec).list;
	__get_cpu_var(tasklet_hi_vec).list = NULL;
	local_irq_enable();

	while (list) {
		struct tasklet_struct *t = list;

		list = list->next;

		if (tasklet_trylock(t)) {
			if (!atomic_read(&t->count)) {
				if (!test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
					BUG();
				t->func(t->data);
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}

		local_irq_disable();
		t->next = __get_cpu_var(tasklet_hi_vec).list;
		__get_cpu_var(tasklet_hi_vec).list = t;
		__raise_softirq_irqoff(HI_SOFTIRQ);
		local_irq_enable();
	}
}

static bool tasklets_inited = false;
static void ensure_tasklets_inited(void)
{
	if (! tasklets_inited) {
		open_softirq(TASKLET_SOFTIRQ, tasklet_action, NULL);
		open_softirq(HI_SOFTIRQ, tasklet_hi_action, NULL);
		tasklets_inited = true;
	}
}

void fastcall __tasklet_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	ensure_tasklets_inited();
	local_irq_save(flags);
	t->next = __get_cpu_var(tasklet_vec).list;
	__get_cpu_var(tasklet_vec).list = t;
	raise_softirq_irqoff(TASKLET_SOFTIRQ);
	local_irq_restore(flags);
}

EXPORT_SYMBOL(__tasklet_schedule);

void fastcall __tasklet_hi_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	ensure_tasklets_inited();
	local_irq_save(flags);
	t->next = __get_cpu_var(tasklet_hi_vec).list;
	__get_cpu_var(tasklet_hi_vec).list = t;
	raise_softirq_irqoff(HI_SOFTIRQ);
	local_irq_restore(flags);
}

EXPORT_SYMBOL(__tasklet_hi_schedule);


void tasklet_init(struct tasklet_struct *t,
		  void (*func)(unsigned long), unsigned long data)
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->func = func;
	t->data = data;
}

EXPORT_SYMBOL(tasklet_init);

void tasklet_kill(struct tasklet_struct *t)
{
	if (in_interrupt())
		printk("Attempt to kill tasklet from interrupt\n");

	while (test_and_set_bit(TASKLET_STATE_SCHED, &t->state)) {
		do
			yield();
		while (test_bit(TASKLET_STATE_SCHED, &t->state));
	}
	tasklet_unlock_wait(t);
	clear_bit(TASKLET_STATE_SCHED, &t->state);
}

EXPORT_SYMBOL(tasklet_kill);
