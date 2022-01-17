/*
 * Copyright (C) 2007-2010, Strawberry Development Group.
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
#include <linux/semaphore.h>
#include <idl/capros/Node.h>
#include <idl/capros/DevPrivs.h>
#include <domain/assert.h>
#include <domain/cmtesync.h>
#include <domain/CMTEThread.h>

#define IntStackSize 1024 // I have no idea what this should be

struct irqDesc {
  struct irqDesc * next;
  unsigned int irqNum;
  void * cookie;
  irq_handler_t handler;
  CMTESemaphore sem;
  unsigned int intThreadNum;
} * descList = NULL;
CMTEMutex_DECLARE_Unlocked(listLock);

void *
interrupt_thread_func(void * threadArg)
{
  result_t result;
  struct irqDesc * desc = (struct irqDesc *)threadArg;

  // Set the preempt_count so in_interrupt() will return true.
  add_preempt_count(HARDIRQ_OFFSET);	// compare with __irq_enter

  for (;;) {
    result = capros_DevPrivs_waitIRQ(KR_DEVPRIVS, desc->irqNum);
    if (result == RC_capros_DevPrivs_AllocFail) {
      // The irq has been deallocated. We will get no more interrupts.
      break;
    }
    if (result != RC_OK) {
      printk("waitIRQ returned 0x%x!\n", result);
      assert(false);
    }

    irqreturn_t irqret = (*desc->handler)(desc->irqNum, desc->cookie);
    (void)irqret;	// irqret isn't used
  }
  CMTESemaphore_up(&desc->sem);	// notify free_irq we are exiting

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

  if (irqflags & IRQF_SHARED) {
    // return -EINVAL;	// We don't support shared interrupts
    // Ignore IRQF_SHARED and hope only one device needs the irq.
    kprintf(KR_OSTREAM, "request_irq: ignoring IRQF_SHARED from %s\n",
            devname);
  }
  if (!handler)
    return -EINVAL;

  result = capros_DevPrivs_allocIRQ(KR_DEVPRIVS, irq, 8 /* priority */);
  if (result) {	// already allocated, or invalid irq number
    if (result == RC_capros_DevPrivs_AllocFail)
      return -EBUSY;
    return -EINVAL;
  }

  struct irqDesc * desc
    = (struct irqDesc *)kmalloc(sizeof(struct irqDesc), GFP_KERNEL);
  if (!desc)
    return -ENOMEM;

  desc->irqNum = irq;
  desc->cookie = dev_id;
  desc->handler = handler;
  CMTESemaphore_init(&desc->sem, 0);

  result_t lthres = CMTEThread_create(IntStackSize, interrupt_thread_func,
             desc, &desc->intThreadNum);
  if (lthres != RC_OK) {
    kfree(desc);
    capros_DevPrivs_releaseIRQ(KR_DEVPRIVS, irq);
    return -ENOMEM;
  }

  CMTEMutex_lock(&listLock);
  desc->next = descList;	// chain into list
  descList = desc;
  CMTEMutex_unlock(&listLock);

  result = capros_DevPrivs_enableIRQ(KR_DEVPRIVS, irq);
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
  // Find the irqDesc.
  // This list will rarely be longer than one element.
  struct irqDesc * desc;
  struct irqDesc * * descp= &descList;
  CMTEMutex_lock(&listLock);
  while (1) {
    desc = *descp;
    if (!desc) {	// reached end of list
      CMTEMutex_unlock(&listLock);
      return;		// nothing to free
    }
    if (desc->irqNum == irq)
      break;		// found the one
    descp = &(desc->next);
  }
  descList = desc->next;	// unchain
  CMTEMutex_unlock(&listLock);

  capros_DevPrivs_releaseIRQ(KR_DEVPRIVS, irq);

  /* When the IRQ is released, the loop in interrupt_thread_func will terminate,
  which will cause that thread to exit. 
  Wait for it to commit to exiting.
  (If we don't wait, the irq could be reassigned before the interrupt thread
  notices.) */
  CMTESemaphore_down(&desc->sem);

  kfree(desc);
}

