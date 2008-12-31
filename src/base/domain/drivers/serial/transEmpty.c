/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Task to wait until the serial port transmission is finished.
*/

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <eros/Invoke.h>
#include <domain/assert.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <domain/CMTEThread.h>
#include "serialPort.h"

unsigned int transmitterEmptyThreadNum = noThread;

DECLARE_WAIT_QUEUE_HEAD(sent_wait_queue_head);

bool isTransmitterEmpty(struct uart_port * port)
{
  if (port->type == PORT_UNKNOWN || port->fifosize == 0)
    return true;

  return port->ops->tx_empty(port);
}

#define transmitterEmptyTaskStackSize 2048

void *
transmitterEmptyTask(void * arg)
{
  Message theMsg;
  Message * msg = &theMsg;
  struct uart_state * state = &theUartState;

  msg->rcv_key0 = msg->rcv_key1 = msg->rcv_key2 = KR_VOID;
  msg->rcv_limit = 0;
  msg->rcv_rsmkey = KR_RETURN;	// Not necessary, we receive one-way messages

  msg->snd_invKey = KR_VOID;
  msg->snd_key0 = msg->snd_key1 = msg->snd_key2 = msg->snd_rsmkey = KR_VOID;
  msg->snd_len = 0;
  /* The void key is not picky about the other parameters,
     so it's OK to leave them uninitialized. */

  for (;;) {
    RETURN(msg);
////kdprintf(KR_OSTREAM, "transmitterEmptyTask invoked.\n");////
    // We get invoked when the queue becomes nonempty.
    struct uart_port * port = state->port;

    /*
     * Set the check interval to be 1/5 of the estimated time to
     * send a single character, and make it at least 1.  The check
     * interval should also be less than the timeout.
     *
     * Note: we have to use pretty tight timings here to satisfy
     * the NIST-PCTS.
     */
    unsigned long char_time = (port->timeout - HZ/50) / port->fifosize;
    char_time = jiffies_to_msecs(char_time);
    char_time = char_time / 5;
    if (char_time == 0)
      char_time = 1;

    /*
     * If the transmitter hasn't cleared in twice the approximate
     * amount of time to send the entire FIFO, it probably won't
     * ever clear.  This assumes the UART isn't doing flow
     * control, which is currently the case.  Hence, if it ever
     * takes longer than port->timeout, this is probably due to a
     * UART bug of some kind.  So, we clamp the timeout parameter at
     * 2*port->timeout.
     */
    unsigned long expire = jiffies + 2 * port->timeout;

    while (! isTransmitterEmpty(port)) {
      capros_Sleep_sleep(KR_SLEEP, char_time);
      if (time_after(jiffies, expire))
        break;		// too long, give up
    }

////kdprintf(KR_OSTREAM, "transmitterEmptyTask waking up queue.\n");////
    /* The transmitter is now empty. Wake up the queue. */
    wake_up(&sent_wait_queue_head);
  }
}

void
WaitForTransmitterEmpty(wait_queue_t * wait)
{
  wait_queue_head_t * q = &sent_wait_queue_head;

  // Like add_wait_queue(q, wait), but also determine if the queue is empty.
  wait->flags &= ~WQ_FLAG_EXCLUSIVE;
  mutex_lock(&q->mutx);

  bool isEmpty = list_empty(&q->task_list);
  __add_wait_queue(q, wait);

  mutex_unlock(&q->mutx);

  if (isEmpty) {
    // Start the transmit waiter process so it will serve the queue.
    Message msg;
    msg.snd_key0 = msg.snd_key1 = msg.snd_key2 = msg.snd_rsmkey = KR_VOID;
    msg.snd_w1 = msg.snd_w2 = msg.snd_w3 = 0;
    msg.snd_code = RC_OK;
    msg.snd_len = 0;
    msg.snd_invKey = KR_XMITWAITER;
    SEND(&msg);
  }
}

int
CreateTransmitterEmptyTask(void)
{
  // Create the transmitterEmpty process.
  result_t result = CMTEThread_create(transmitterEmptyTaskStackSize,
                      &transmitterEmptyTask, NULL, &transmitterEmptyThreadNum);
  if (result != RC_OK) {
    return -ENOMEM;
  }

  // Get process key
  result = capros_Node_getSlotExtended(KR_KEYSTORE,
                      LKSN_THREAD_PROCESS_KEYS + transmitterEmptyThreadNum,
                      KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Process_makeStartKey(KR_TEMP0, 0, KR_XMITWAITER);
  assert(result == RC_OK);
  return 0;
}

void
DestroyTransmitterEmptyTask(void)
{
  CMTEThread_destroy(transmitterEmptyThreadNum);
  transmitterEmptyThreadNum = noThread;
}

void
TransmitterEmptySepuku(void)
{
  transmitterEmptyThreadNum = noThread;
  CMTEThread_exit();
}
