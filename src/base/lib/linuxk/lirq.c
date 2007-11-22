/*
 * Copyright (C) 2007, Strawberry Development Group.
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

/* Emulation for Linux procedures request_irq and free_irq.
*/

#include <eros/Invoke.h>	// get RC_OK
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <asm-generic/semaphore.h>
#include <idl/capros/Node.h>
#include <idl/capros/DevPrivs.h>
#include <domain/assert.h>

#define IntStackSize 1024 // I have no idea what this should be

struct interrupt_thread_params {
  unsigned long irqNum;
  void * cookie;
  irq_handler_t handler;
};

struct interrupt_thread_args {
  struct semaphore arglock;		// to pass the args safely
  struct interrupt_thread_params params;
};

void *
interrupt_thread_func(void * cookie)
{
  result_t result;

  // Set the preempt_count so in_interrupt() will return true.
  add_preempt_count(HARDIRQ_OFFSET);	// compare with __irq_enter

  struct interrupt_thread_args * args
    = (struct interrupt_thread_args *)cookie;
  struct interrupt_thread_params params = args->params;
  up(&args->arglock);		// we are done with args

  /* KR_LINUX_EMUL slot LE_DEVPRIVS has a key to the DevPrivs key. */
  result = capros_Node_getSlot(KR_LINUX_EMUL, LE_DEVPRIVS, KR_TEMP0);
  assert(result == RC_OK);

  for (;;) {
    result = capros_DevPrivs_waitIRQ(KR_TEMP0, params.irqNum);
    if (result == RC_capros_DevPrivs_AllocFail)
      // The irq has been deallocated. We will get no more interrupts.
      break;
    assert(result == RC_OK);

    irqreturn_t irqret = (*params.handler)(params.irqNum, params.cookie);
    (void)irqret;	// irqret isn't used
  }

  return 0;
}

/**
 *	request_irq - allocate an interrupt line
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs
 *	@irqflags: Interrupt type flags
 *	@devname: An ascii name for the claiming device
 *	@dev_id: A cookie passed back to the handler function
 *
 *	This call allocates interrupt resources and enables the
 *	interrupt line and IRQ handling. From the point this
 *	call is made your handler function may be invoked. Since
 *	your handler function must clear any interrupt the board
 *	raises, you must take care both to initialise your hardware
 *	and to set up the interrupt handler in the right order.
 *
 *	dev_id must be globally unique. Normally the address of the
 *	device data structure is used as the cookie. Since the handler
 *	receives this value it makes sense to use it.
 *
 *	Flags:
 *
 *	IRQF_SHARED		Interrupt is shared
 *	IRQF_DISABLED	Disable local interrupts while processing
 *	IRQF_SAMPLE_RANDOM	The interrupt can be used for entropy
 *
 */

int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long irqflags, const char * devname, void * dev_id)
{
  result_t result;

  if (irqflags & IRQF_SHARED)
    return -EINVAL;	// We don't support shared interrupts
  if (!handler)
    return -EINVAL;

  /* KR_LINUX_EMUL slot LE_DEVPRIVS has a key to the DevPrivs key. */
  result = capros_Node_getSlot(KR_LINUX_EMUL, LE_DEVPRIVS, KR_TEMP0);
  assert(result == RC_OK);

  result = capros_DevPrivs_allocIRQ(KR_TEMP0, irq, 8 /* priority */);
  if (result)
    return -EINVAL;

  struct interrupt_thread_args args = {
    .arglock = __SEMAPHORE_INIT(args.arglock, 0),	// initially locked
    .params = {
      .irqNum = irq,
      .cookie = dev_id,
      .handler = handler
    }
  };
  unsigned int newThreadNum;

  result_t lthres = lthread_new_thread(IntStackSize, interrupt_thread_func,
             &args, &newThreadNum);
  down(&args.arglock);

  result = capros_Node_getSlot(KR_LINUX_EMUL, LE_DEVPRIVS, KR_TEMP0);
  assert(result == RC_OK);

  if (lthres) {
    capros_DevPrivs_releaseIRQ(KR_TEMP0, irq);
    return -EINVAL;
  }

  result = capros_DevPrivs_enableIRQ(KR_TEMP0, irq);
  assert(result == RC_OK);

  return 0;
}

/**
 *	free_irq - free an interrupt
 *	@irq: Interrupt line to free
 *	@dev_id: Device identity to free
 *
 *	Remove an interrupt handler. The handler is removed and if the
 *	interrupt line is no longer in use by any driver it is disabled.
 */
void free_irq(unsigned int irq, void *dev_id)
{
  result_t result;

  /* KR_LINUX_EMUL slot LE_DEVPRIVS has a key to the DevPrivs key. */
  result = capros_Node_getSlot(KR_LINUX_EMUL, LE_DEVPRIVS, KR_TEMP0);
  if (result == RC_OK)
    result = capros_DevPrivs_releaseIRQ(KR_TEMP0, irq);

  /* When the IRQ is released, interrupt_thread_func will return,
  which will cause that thread to exit. */
}

