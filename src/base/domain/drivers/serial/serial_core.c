/*
 *  Copyright 1999 ARM Limited
 *  Copyright (C) 2000-2001 Deep Blue Solutions Ltd.
 * Copyright (C) 2007, Strawberry Development Group.
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

/* Emulation for Linux procedures uart_*.
*/

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/serial_core.h>
#include <linux/tty_flip.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <asm-generic/semaphore.h>
#include <asm/termios.h>
#include <linux/thread_info.h>
#include <eros/Invoke.h>	// get RC_OK
#include <domain/assert.h>
#include <idl/capros/SerialPort.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Process.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/DevPrivs.h>

#include <linux/amba/bus.h>
#include <linux/amba/serial.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <eros/arch/arm/mach-ep93xx/ep9315-syscon.h>

#define TTY_THRESHOLD_THROTTLE   128
#define TTY_THRESHOLD_UNTHROTTLE 128

/*
 * This is used to lock changes in serial line configuration.
 */
static DEFINE_MUTEX(port_mutex);

#define msgRcvBufSize UART_XMIT_SIZE
unsigned char msgRcvBuf[msgRcvBufSize];

typedef char capKludge;	/* The ADDRESS of a capKludge is the index of
	a key in KR_KEYSTORE. */

/* xmitWaiter may have a resume key to the client that 
transmitted the data in xmitBuf. */
capKludge xmitWaiter;
#define toCap(x) ((cap_t)&x)
#define xmitWaiterCap (toCap(xmitWaiter))

/*
 * lockdep: port->lock is initialized in two places, but we
 *          want only one lock-class:
 */
static struct lock_class_key port_lock_key;

struct ktermios theTermios = {
  .c_iflag = ICRNL | IXON,
  .c_cflag = BOTHER | CS8 | CREAD | CLOCAL,
  .c_cc = INIT_C_CC,
  .c_ospeed = 9600
  };
struct uart_state theUartState = {
  .mutex = __MUTEX_INITIALIZER(theUartState.mutex)
  };
struct uart_driver theUartDriver = {
  };
struct tty_struct theTtyStruct = {
  .index = 0,
  .driver_data = &theUartState,
  .termios = &theTermios
  };
struct tty_driver theTtyDriver = {
  .magic = TTY_DRIVER_MAGIC,
  .num = 1,
  .type		= TTY_DRIVER_TYPE_SERIAL,
  .subtype	= SERIAL_TYPE_NORMAL,
  .flags	= TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV
  };

#if 0
#define HIGH_BITS_OFFSET	((sizeof(long)-sizeof(int))*8)

#define uart_users(state)	((state)->count + ((state)->info ? (state)->info->blocked_open : 0))
#endif

#define uart_console(port)	(0)

static void uart_change_speed(struct uart_state *state, struct ktermios *old_termios);
static void uart_wait_until_sent(struct tty_struct *tty, int timeout);
static void uart_change_pm(struct uart_state *state, int pm_state);

struct wait_break_data {
  bool inUse;
  capKludge waiter;
} brkGlobal;

int
uart_handle_break(struct uart_port * port)
{
  if (brkGlobal.inUse) {
    // Return to the waiter.
    capros_Node_getSlotExtended(KR_KEYSTORE, toCap(brkGlobal.waiter), KR_TEMP0);
    Message msg;
    msg.snd_key0 = msg.snd_key1 = msg.snd_key2 = msg.snd_rsmkey = KR_VOID;
    msg.snd_w1 = msg.snd_w2 = msg.snd_w3 = 0;
    msg.snd_code = RC_OK;
    msg.snd_len = 0;
    msg.snd_invKey = KR_TEMP0;
    PSEND(&msg);	// prompt send

    brkGlobal.inUse = false;
  }
  return 0;
}

/* dcd changes are reported to the delta_msr_wait wait queue.
   They are not separately handled. */
void
uart_handle_dcd_change(struct uart_port *port, unsigned int status)
{
}

/**
 *	uart_handle_cts_change - handle a change of clear-to-send state
 *	@port: uart_port structure for the open port
 *	@status: new clear to send status, nonzero if active
 */
void
uart_handle_cts_change(struct uart_port *port, unsigned int status)
{
	struct uart_info *info = port->info;
	struct tty_struct *tty = info->tty;

	port->icount.cts++;

	if (info->flags & UIF_CTS_FLOW) {
		if (tty->hw_stopped) {
			if (status) {
				tty->hw_stopped = 0;
				port->ops->start_tx(port);
				uart_write_wakeup(port);
			}
		} else {
			if (!status) {
				tty->hw_stopped = 1;
				port->ops->stop_tx(port);
			}
		}
	}
}

void uart_write_wakeup(struct uart_port *port)
{
	struct uart_info *info = port->info;
	/*
	 * This means you called this function _after_ the port was
	 * closed.  No cookie for you.
	 */
	BUG_ON(!info);
	wake_up_interruptible(&info->tty->write_wait);
}

static void uart_stop(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	port->ops->stop_tx(port);
	spin_unlock_irqrestore(&port->lock, flags);
}

static void __uart_start(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;

	if (!uart_circ_empty(&state->info->xmit) && state->info->xmit.buf &&
	    !tty->stopped && !tty->hw_stopped)
		port->ops->start_tx(port);
}

static void uart_start(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	__uart_start(tty);
	spin_unlock_irqrestore(&port->lock, flags);
}

#if 0
static void uart_tasklet_action(unsigned long data)
{
	struct uart_state *state = (struct uart_state *)data;
	tty_wakeup(state->info->tty);
}
#endif


static inline void
uart_update_mctrl(struct uart_port *port, unsigned int set, unsigned int clear)
{
	unsigned long flags;
	unsigned int old;

	spin_lock_irqsave(&port->lock, flags);
	old = port->mctrl;
	port->mctrl = (old & ~clear) | set;
	if (old != port->mctrl)
		port->ops->set_mctrl(port, port->mctrl);
	spin_unlock_irqrestore(&port->lock, flags);
}

#define uart_set_mctrl(port,set)	uart_update_mctrl(port,set,0)
#define uart_clear_mctrl(port,clear)	uart_update_mctrl(port,0,clear)

/*
 * Startup the port.  This will be called once per open.  All calls
 * will be serialised by the per-port semaphore.
 */
static int uart_startup(struct uart_state *state, int init_hw)
{
	struct uart_info *info = state->info;
	struct uart_port *port = state->port;
	int retval = 0;

	if (info->flags & UIF_INITIALIZED)
		return 0;

	/*
	 * Set the TTY IO error marker - we will only clear this
	 * once we have successfully opened the port.
	 */
	set_bit(TTY_IO_ERROR, &info->tty->flags);

	if (port->type == PORT_UNKNOWN)
		return 0;

	/*
	 * Initialise and allocate the transmit and message
	 * buffers.
	 */
	if (!info->xmit.buf) {
		void * buf = kmalloc(UART_XMIT_SIZE, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		info->xmit.buf = (char *) buf;
		uart_circ_clear(&info->xmit);
	}

	retval = port->ops->startup(port);
	if (retval == 0) {
		if (init_hw) {
			/*
			 * Initialise the hardware port settings.
			 */
			uart_change_speed(state, NULL);

			/*
			 * Setup the RTS and DTR signals once the
			 * port is open and ready to respond.
			 */
			if (info->tty->termios->c_cflag & CBAUD)
				uart_set_mctrl(port, TIOCM_RTS | TIOCM_DTR);
		}

		if (info->flags & UIF_CTS_FLOW) {
			spin_lock_irq(&port->lock);
			if (!(port->ops->get_mctrl(port) & TIOCM_CTS))
				info->tty->hw_stopped = 1;
			spin_unlock_irq(&port->lock);
		}

		info->flags |= UIF_INITIALIZED;

		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	}

	return retval;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.  Calls to
 * uart_shutdown are serialised by the per-port semaphore.
 */
static void uart_shutdown(struct uart_state *state)
{
	struct uart_info *info = state->info;
	struct uart_port *port = state->port;

	/*
	 * Set the TTY IO error marker
	 */
	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	if (info->flags & UIF_INITIALIZED) {
		info->flags &= ~UIF_INITIALIZED;

		/*
		 * clear delta_msr_wait queue to avoid mem leaks: we may free
		 * the irq here so the queue might never be woken up.  Note
		 * that we won't end up waiting on delta_msr_wait again since
		 * any outstanding file descriptors should be pointing at
		 * hung_up_tty_fops now.
		 */
		wake_up_interruptible(&info->delta_msr_wait);

		/*
		 * Free the IRQ and disable the port.
		 */
		port->ops->shutdown(port);

		/*
		 * Ensure that the IRQ handler isn't running on another CPU.
		 */
		synchronize_irq(port->irq);
	}

	/*
	 * Free the transmit buffers.
	 */
	if (info->xmit.buf) {
		kfree(info->xmit.buf);
		info->xmit.buf = NULL;
	}
}

/**
 *	uart_update_timeout - update per-port FIFO timeout.
 *	@port:  uart_port structure describing the port
 *	@cflag: termios cflag value
 *	@baud:  speed of the port
 *
 *	Set the port FIFO timeout value.  The @cflag value should
 *	reflect the actual hardware settings.
 */
void
uart_update_timeout(struct uart_port *port, unsigned int cflag,
		    unsigned int baud)
{
	unsigned int bits;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5:
		bits = 7;
		break;
	case CS6:
		bits = 8;
		break;
	case CS7:
		bits = 9;
		break;
	default:
		bits = 10;
		break; // CS8
	}

	if (cflag & CSTOPB)
		bits++;
	if (cflag & PARENB)
		bits++;

	/*
	 * The total number of bits to be transmitted in the fifo.
	 */
	bits = bits * port->fifosize;

	/*
	 * Figure the timeout to send the above number of bits.
	 * Add .02 seconds of slop
	 */
	port->timeout = (HZ * bits) / baud + HZ/50;
}

/**
 *	uart_get_baud_rate - return baud rate for a particular port
 *	@port: uart_port structure describing the port in question.
 *	@termios: desired termios settings.
 *	@old: old termios (or NULL)
 *	@min: minimum acceptable baud rate
 *	@max: maximum acceptable baud rate
 *
 *	Decode the termios structure into a numeric baud rate,
 *	taking account of the magic 38400 baud rate (with spd_*
 *	flags), and mapping the %B0 rate to 9600 baud.
 *
 *	If the new baud rate is invalid, try the old termios setting.
 *	If it's still invalid, we try 9600 baud.
 *
 *	Update the @termios structure to reflect the baud rate
 *	we're actually going to be using.
 */
unsigned int
uart_get_baud_rate(struct uart_port *port, struct ktermios *termios,
		   struct ktermios *old, unsigned int min, unsigned int max)
{
	unsigned int try, baud;

	for (try = 0; try < 2; try++) {
		baud = tty_termios_baud_rate(termios);

		/*
		 * Special case: B0 rate.
		 */
		if (baud == 0)
			baud = 9600;

		if (baud >= min && baud <= max)
			return baud;

		/*
		 * As a last resort, if the quotient is zero,
		 * default to 9600 bps
		 */
		termios->c_cflag |= BOTHER;
		termios->c_ospeed = 9600;
	}

	return 0;
}

/**
 *	uart_get_divisor - return uart clock divisor
 *	@port: uart_port structure describing the port.
 *	@baud: desired baud rate
 *
 *	Calculate the uart clock divisor for the port.
 */
unsigned int
uart_get_divisor(struct uart_port *port, unsigned int baud)
{
	unsigned int quot;

	/*
	 * Old custom speed handling.
	 */
	if (baud == 38400 && (port->flags & UPF_SPD_MASK) == UPF_SPD_CUST)
		quot = port->custom_divisor;
	else
		quot = (port->uartclk + (8 * baud)) / (16 * baud);

	return quot;
}

static void
uart_change_speed(struct uart_state *state, struct ktermios *old_termios)
{
	struct tty_struct *tty = state->info->tty;
	struct uart_port *port = state->port;
	struct ktermios *termios;

	/*
	 * If we have no tty, termios, or the port does not exist,
	 * then we can't set the parameters for this port.
	 */
	if (!tty || !tty->termios || port->type == PORT_UNKNOWN)
		return;

	termios = tty->termios;

	/*
	 * Set flags based on termios cflag
	 */
	if (termios->c_cflag & CRTSCTS)
		state->info->flags |= UIF_CTS_FLOW;
	else
		state->info->flags &= ~UIF_CTS_FLOW;

	port->ops->set_termios(port, termios, old_termios);
}

/* Returns number of characters written (possibly 0), or -EL3HLT. */
static int
uart_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port;
	struct circ_buf *circ;
	unsigned long flags;
	int c, ret = 0;

	/*
	 * This means you called this function _after_ the port was
	 * closed.  No cookie for you.
	 */
	if (!state || !state->info) {
		// WARN_ON(1);
		return -EL3HLT;
	}

	port = state->port;
	circ = &state->info->xmit;

	if (!circ->buf)
		return 0;

	spin_lock_irqsave(&port->lock, flags);
	while (1) {
		c = CIRC_SPACE_TO_END(circ->head, circ->tail, UART_XMIT_SIZE);
		if (count < c)
			c = count;
		if (c <= 0)
			break;
		memcpy(circ->buf + circ->head, buf, c);
		circ->head = (circ->head + c) & (UART_XMIT_SIZE - 1);
		buf += c;
		count -= c;
		ret += c;
	}
	spin_unlock_irqrestore(&port->lock, flags);

	uart_start(tty);
	return ret;
}

#if 0
static int uart_write_room(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;

	return uart_circ_chars_free(&state->info->xmit);
}

static int uart_chars_in_buffer(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;

	return uart_circ_chars_pending(&state->info->xmit);
}
#endif

static void uart_flush_buffer(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;
	unsigned long flags;

	/*
	 * This means you called this function _after_ the port was
	 * closed.  No cookie for you.
	 */
	if (!state || !state->info) {
		// WARN_ON(1);
		return;
	}

	spin_lock_irqsave(&port->lock, flags);
	uart_circ_clear(&state->info->xmit);
	spin_unlock_irqrestore(&port->lock, flags);
	//// wake up xmit waiter
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void uart_send_xchar(struct tty_struct *tty, char ch)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;
	unsigned long flags;

	if (port->ops->send_xchar)
		port->ops->send_xchar(port, ch);
	else {
		port->x_char = ch;
		if (ch) {
			spin_lock_irqsave(&port->lock, flags);
			port->ops->start_tx(port);
			spin_unlock_irqrestore(&port->lock, flags);
		}
	}
}

static void uart_throttle(struct tty_struct *tty)
{
  if (!test_and_set_bit(TTY_THROTTLED, &tty->flags)) {
	if (I_IXOFF(tty))
		uart_send_xchar(tty, STOP_CHAR(tty));

	if (tty->termios->c_cflag & CRTSCTS) {
		struct uart_state *state = tty->driver_data;
		uart_clear_mctrl(state->port, TIOCM_RTS);
	}
  }
}

static void uart_unthrottle(struct tty_struct *tty)
{
  if (test_and_clear_bit(TTY_THROTTLED, &tty->flags)) {
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;

	if (I_IXOFF(tty)) {
		if (port->x_char)
			port->x_char = 0;
		else
			uart_send_xchar(tty, START_CHAR(tty));
	}

	if (tty->termios->c_cflag & CRTSCTS)
		uart_set_mctrl(port, TIOCM_RTS);
  }
}

#if 0
static int uart_get_info(struct uart_state *state,
			 struct serial_struct __user *retinfo)
{
	struct uart_port *port = state->port;
	struct serial_struct tmp;

	memset(&tmp, 0, sizeof(tmp));
	tmp.type	    = port->type;
	tmp.line	    = port->line;
	tmp.port	    = port->iobase;
	if (HIGH_BITS_OFFSET)
		tmp.port_high = (long) port->iobase >> HIGH_BITS_OFFSET;
	tmp.irq		    = port->irq;
	tmp.flags	    = port->flags;
	tmp.xmit_fifo_size  = port->fifosize;
	tmp.baud_base	    = port->uartclk / 16;
	tmp.close_delay	    = state->close_delay / 10;
	tmp.closing_wait    = state->closing_wait == USF_CLOSING_WAIT_NONE ?
				ASYNC_CLOSING_WAIT_NONE :
			        state->closing_wait / 10;
	tmp.custom_divisor  = port->custom_divisor;
	tmp.hub6	    = port->hub6;
	tmp.io_type         = port->iotype;
	tmp.iomem_reg_shift = port->regshift;
	tmp.iomem_base      = (void *)port->mapbase;

	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

/*
 * uart_get_lsr_info - get line status register info.
 * Note: uart_ioctl protects us against hangups.
 */
static int uart_get_lsr_info(struct uart_state *state,
			     unsigned int __user *value)
{
	struct uart_port *port = state->port;
	unsigned int result;

	result = port->ops->tx_empty(port);

	/*
	 * If we're about to load something into the transmit
	 * register, we'll pretend the transmitter isn't empty to
	 * avoid a race condition (depending on when the transmit
	 * interrupt happens).
	 */
	if (port->x_char ||
	    ((uart_circ_chars_pending(&state->info->xmit) > 0) &&
	     !state->info->tty->stopped && !state->info->tty->hw_stopped))
		result &= ~TIOCSER_TEMT;
	
	return put_user(result, value);
}
#endif

static int uart_tiocmget(struct tty_struct *tty, struct file *file)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;
	int result = -EIO;

	mutex_lock(&state->mutex);
	if (
	    !(tty->flags & (1 << TTY_IO_ERROR))) {
		result = port->mctrl;

		spin_lock_irq(&port->lock);
		result |= port->ops->get_mctrl(port);
		spin_unlock_irq(&port->lock);
	}
	mutex_unlock(&state->mutex);

	return result;
}

static int
uart_tiocmset(struct tty_struct *tty, struct file *file,
	      unsigned int set, unsigned int clear)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;
	int ret = -EIO;

	mutex_lock(&state->mutex);
	if (
	    !(tty->flags & (1 << TTY_IO_ERROR))) {
		uart_update_mctrl(port, set, clear);
		ret = 0;
	}
	mutex_unlock(&state->mutex);
	return ret;
}

static void uart_break_ctl(struct tty_struct *tty, int break_state)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;

	BUG_ON(!kernel_locked());

	mutex_lock(&state->mutex);

	if (port->type != PORT_UNKNOWN)
		port->ops->break_ctl(port, break_state);

	mutex_unlock(&state->mutex);
}

#if 0
static int uart_do_autoconfig(struct uart_state *state)
{
	struct uart_port *port = state->port;
	int flags, ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	/*
	 * Take the per-port semaphore.  This prevents count from
	 * changing, and hence any extra opens of the port while
	 * we're auto-configuring.
	 */
	if (mutex_lock_interruptible(&state->mutex))
		return -ERESTARTSYS;

	ret = -EBUSY;
	if (uart_users(state) == 1) {
		uart_shutdown(state);

		/*
		 * If we already have a port type configured,
		 * we must release its resources.
		 */
		if (port->type != PORT_UNKNOWN)
			port->ops->release_port(port);

		flags = UART_CONFIG_TYPE;
		if (port->flags & UPF_AUTO_IRQ)
			flags |= UART_CONFIG_IRQ;

		/*
		 * This will claim the ports resources if
		 * a port is found.
		 */
		port->ops->config_port(port, flags);

		ret = uart_startup(state, 1);
	}
	mutex_unlock(&state->mutex);
	return ret;
}
#endif

/* Copy interrupt counters from port structure to capros structure. */
static void
icount_port_to_capros(const struct uart_icount * from,
                      struct capros_SerialPort_icounter * icnt)
{
  icnt->cts         = from->cts;
  icnt->dsr         = from->dsr;
  icnt->rng         = from->rng;
  icnt->dcd         = from->dcd;
  icnt->rx          = from->rx;
  icnt->tx          = from->tx;
  icnt->frame       = from->frame;
  icnt->overrun     = from->overrun;
  icnt->parity      = from->parity;
  icnt->brk         = from->brk;
  icnt->buf_overrun = from->buf_overrun;
}

static bool
msrIsChanged(struct capros_SerialPort_icounter * cprev,
             struct capros_SerialPort_icounter * cnew,
             unsigned long mask)
{
  return (((mask & TIOCM_RNG) && (cnew->rng != cprev->rng)) ||
          ((mask & TIOCM_DSR) && (cnew->dsr != cprev->dsr)) ||
          ((mask & TIOCM_CD)  && (cnew->dcd != cprev->dcd)) ||
          ((mask & TIOCM_CTS) && (cnew->cts != cprev->cts)));
}

struct wait_modem_data {
  struct uart_state * state;
  unsigned long mask;
  struct capros_SerialPort_icounter cprev;
  bool inUse;
  capKludge waiter;
} wmdGlobal;

wait_queue_t msrWaitQ;

/* This is called while holding the port lock. */
static int
wait_modem_func(wait_queue_t * wait, unsigned mode, int sync, void * key)
{
  struct wait_modem_data * wmd = wait->private;
  struct uart_state * state = wmd->state;
  struct uart_port *port = state->port;
  struct capros_SerialPort_icounter cnew;

  icount_port_to_capros(&port->icount, &cnew);
  if (msrIsChanged(&wmd->cprev, &cnew, wmd->mask)) {
    // Return to the caller.
    capros_Node_getSlotExtended(KR_KEYSTORE, toCap(wmd->waiter), KR_TEMP0);
    Message msg;
    msg.snd_key0 = msg.snd_key1 = msg.snd_key2 = msg.snd_rsmkey = KR_VOID;
    msg.snd_w1 = msg.snd_w2 = msg.snd_w3 = 0;
    msg.snd_code = RC_OK;
    msg.snd_data = &cnew;
    msg.snd_len = sizeof(cnew);
    msg.snd_invKey = KR_TEMP0;
    PSEND(&msg);	// prompt send

    remove_wait_queue(&wmd->state->info->delta_msr_wait, wait);
  }

  return 1;
}

/*
 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
 * - mask passed in arg for lines of interest
 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)

   Returns 2 if someone already waiting,
   1 if returning immediately, 0 if waiting.
 */
static int
uart_wait_modem_status(struct uart_state * state,
                       const struct capros_SerialPort_icounter * cprevp,
                       struct capros_SerialPort_icounter * cnew,
                       unsigned long arg)
{
  struct uart_port *port = state->port;
  struct wait_modem_data * wmd = &wmdGlobal;

  if (wmd->inUse) return 2;

  /* Make a copy of cprev, in case cprevp and cnew point to the 
     same memory. */
  memcpy(&wmd->cprev, cprevp, sizeof(struct capros_SerialPort_icounter));

  spin_lock_irq(&port->lock);

  // Force modem status interrupts on
  port->ops->enable_ms(port);

  // Can we return immediately?
  icount_port_to_capros(&port->icount, cnew);
  if (msrIsChanged(&wmd->cprev, cnew, arg)) {
    spin_unlock_irq(&port->lock);
    return 1;
  }

  // Set up to wait.
  msrWaitQ.func = &wait_modem_func;
  msrWaitQ.private = wmd;
  INIT_LIST_HEAD(&msrWaitQ.task_list);
  wmd->state = state;
  wmd->mask = arg;
  wmd->inUse = true;
  capros_Node_swapSlotExtended(KR_KEYSTORE, toCap(wmd->waiter),
                               KR_RETURN, KR_VOID);
  add_wait_queue(&state->info->delta_msr_wait, &msrWaitQ);

  spin_unlock_irq(&port->lock);

  return 0;
}

/*
 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
 * Return: write counters to the user passed counter struct
 * NB: both 1->0 and 0->1 transitions are counted except for
 *     RI where only 0->1 is counted.
 */
static void
uart_get_count(struct uart_state * state,
	       struct capros_SerialPort_icounter * icnt)
{
	struct uart_port *port = state->port;

	spin_lock_irq(&port->lock);
	icount_port_to_capros(&port->icount, icnt);
	spin_unlock_irq(&port->lock);
}

#if 0
/*
 * Called via sys_ioctl under the BKL.  We can use spin_lock_irq() here.
 */
static int
uart_ioctl(struct tty_struct *tty, struct file *filp, unsigned int cmd,
	   unsigned long arg)
{
	struct uart_state *state = tty->driver_data;
	void __user *uarg = (void __user *)arg;
	int ret = -ENOIOCTLCMD;

	BUG_ON(!kernel_locked());

	/*
	 * These ioctls don't rely on the hardware to be present.
	 */
	switch (cmd) {
	case TIOCGSERIAL:
		ret = uart_get_info(state, uarg);
		break;

	case TIOCSERCONFIG:
		ret = uart_do_autoconfig(state);
		break;

	case TIOCSERGWILD: /* obsolete */
	case TIOCSERSWILD: /* obsolete */
		ret = 0;
		break;
	}

	if (ret != -ENOIOCTLCMD)
		goto out;

	if (tty->flags & (1 << TTY_IO_ERROR)) {
		ret = -EIO;
		goto out;
	}

	/*
	 * The following should only be used when hardware is present.
	 */
	switch (cmd) {
	case TIOCMIWAIT:
		ret = uart_wait_modem_status(state, arg);
		break;

	case TIOCGICOUNT:
		ret = uart_get_count(state, uarg);
		break;
	}

	if (ret != -ENOIOCTLCMD)
		goto out;

	mutex_lock(&state->mutex);

	if (tty_hung_up_p(filp)) {
		ret = -EIO;
		goto out_up;
	}

	/*
	 * All these rely on hardware being present and need to be
	 * protected against the tty being hung up.
	 */
	switch (cmd) {
	case TIOCSERGETLSR: /* Get line status register */
		ret = uart_get_lsr_info(state, uarg);
		break;

	default: {
		struct uart_port *port = state->port;
		if (port->ops->ioctl)
			ret = port->ops->ioctl(port, cmd, arg);
		break;
	}
	}
 out_up:
	mutex_unlock(&state->mutex);
 out:
	return ret;
}
#endif

static void uart_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	struct uart_state *state = tty->driver_data;
	unsigned long flags;
	unsigned int cflag = tty->termios->c_cflag;

	BUG_ON(!kernel_locked());

	/*
	 * These are the bits that are used to setup various
	 * flags in the low level driver.
	 */
#define RELEVANT_IFLAG(iflag)	((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	if ((cflag ^ old_termios->c_cflag) == 0 &&
	    RELEVANT_IFLAG(tty->termios->c_iflag ^ old_termios->c_iflag) == 0
            && tty->termios->c_ospeed == old_termios->c_ospeed )
		return;	// no relevant change

	uart_change_speed(state, old_termios);

	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) && !(cflag & CBAUD))
		uart_clear_mctrl(state->port, TIOCM_RTS | TIOCM_DTR);

	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) && (cflag & CBAUD)) {
		unsigned int mask = TIOCM_DTR;
		if (!(cflag & CRTSCTS) ||
		    !test_bit(TTY_THROTTLED, &tty->flags))
			mask |= TIOCM_RTS;
		uart_set_mctrl(state->port, mask);
	}

	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) && !(cflag & CRTSCTS)) {
		spin_lock_irqsave(&state->port->lock, flags);
		tty->hw_stopped = 0;
		__uart_start(tty);
		spin_unlock_irqrestore(&state->port->lock, flags);
	}

	/* Handle turning on CRTSCTS */
	if (!(old_termios->c_cflag & CRTSCTS) && (cflag & CRTSCTS)) {
		spin_lock_irqsave(&state->port->lock, flags);
		if (!(state->port->ops->get_mctrl(state->port) & TIOCM_CTS)) {
			tty->hw_stopped = 1;
			state->port->ops->stop_tx(state->port);
		}
		spin_unlock_irqrestore(&state->port->lock, flags);
	}
}

/*
 * In 2.4.5, calls to this will be serialized via the BKL in
 *  linux/drivers/char/tty_io.c:tty_release()
 *  linux/drivers/char/tty_io.c:do_tty_handup()
 */
static void uart_close(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port;
	
	assert(state);
	if (!state->port)
		return;

	port = state->port;

	pr_debug("uart_close(%d) called\n", port->line);

	mutex_lock(&state->mutex);

	--state->count;		// it's now zero

        uart_flush_buffer(tty);

	/*
	 * At this point, we stop accepting input.  To do this, we
	 * disable the receive line status interrupts.
	 */
	if (state->info->flags & UIF_INITIALIZED) {
		unsigned long flags;
		spin_lock_irqsave(&port->lock, flags);
		port->ops->stop_rx(port);
		spin_unlock_irqrestore(&port->lock, flags);
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		uart_wait_until_sent(tty, port->timeout);
	}

	uart_shutdown(state);

	// tty_ldisc_flush(tty);	wake up read waiter////
	
	state->info->tty = NULL;

	if (!uart_console(port)) {
		uart_change_pm(state, 3);
	}

	state->info->flags &= ~UIF_NORMAL_ACTIVE;

	mutex_unlock(&state->mutex);
}

static void uart_wait_until_sent(struct tty_struct * tty, int timeout)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->port;
	unsigned long char_time, expire;

	BUG_ON(!kernel_locked());

	if (port->type == PORT_UNKNOWN || port->fifosize == 0)
		return;

	/*
	 * Set the check interval to be 1/5 of the estimated time to
	 * send a single character, and make it at least 1.  The check
	 * interval should also be less than the timeout.
	 *
	 * Note: we have to use pretty tight timings here to satisfy
	 * the NIST-PCTS.
	 */
	char_time = (port->timeout - HZ/50) / port->fifosize;
	char_time = char_time / 5;
	if (char_time == 0)
		char_time = 1;
	if (timeout && timeout < char_time)
		char_time = timeout;

	/*
	 * If the transmitter hasn't cleared in twice the approximate
	 * amount of time to send the entire FIFO, it probably won't
	 * ever clear.  This assumes the UART isn't doing flow
	 * control, which is currently the case.  Hence, if it ever
	 * takes longer than port->timeout, this is probably due to a
	 * UART bug of some kind.  So, we clamp the timeout parameter at
	 * 2*port->timeout.
	 */
	if (timeout == 0 || timeout > 2 * port->timeout)
		timeout = 2 * port->timeout;

	expire = jiffies + timeout;

	/*
	 * Check whether the transmitter is empty every 'char_time'.
	 * 'timeout' / 'expire' give us the maximum amount of time
	 * we wait.
	 */
	while (!port->ops->tx_empty(port)) {
		msleep_interruptible(jiffies_to_msecs(char_time));
		if (signal_pending(current))
			break;
		if (time_after(jiffies, expire))
			break;
	}
}

/*
 * Copy across the serial console cflag setting into the termios settings
 * for the initial open of the port.  This allows continuity between the
 * kernel settings, and the settings init adopts when it opens the port
 * for the first time.
 */
static void uart_update_termios(struct uart_state *state)
{
	struct tty_struct *tty = state->info->tty;
	struct uart_port *port = state->port;

	/*
	 * If the device failed to grab its irq resources,
	 * or some other error occurred, don't try to talk
	 * to the port hardware.
	 */
	if (!(tty->flags & (1 << TTY_IO_ERROR))) {
		/*
		 * Make termios settings take effect.
		 */
		uart_change_speed(state, NULL);

		/*
		 * And finally enable the RTS and DTR signals.
		 */
		if (tty->termios->c_cflag & CBAUD)
			uart_set_mctrl(port, TIOCM_DTR | TIOCM_RTS);
	}
}

static int
uart_get(struct uart_state * state)
{
	int ret = 0;

	mutex_lock(&state->mutex);

	if (!state->port || state->port->flags & UPF_DEAD) {
		ret = -ENXIO;
		goto err_unlock;
	}

	if (!state->info) {
		state->info = kzalloc(sizeof(struct uart_info), GFP_KERNEL);
		if (state->info) {
			init_waitqueue_head(&state->info->open_wait);
			init_waitqueue_head(&state->info->delta_msr_wait);

			/*
			 * Link the info into the other structures.
			 */
			state->port->info = state->info;
		} else {
			ret = -ENOMEM;
			goto err_unlock;
		}
	}
	return 0;

 err_unlock:
	mutex_unlock(&state->mutex);
	return ret;
}

/*
 * In 2.4.5, calls to uart_open are serialised by the BKL in
 *   linux/fs/devices.c:chrdev_open()
 * Note that if this fails, then uart_close() _will_ be called.
 */
static int uart_open(struct tty_struct *tty)
{
	struct uart_state * state = tty->driver_data;
	int retval, line = 0;

	BUG_ON(!kernel_locked());
	pr_debug("uart_open(%d) called\n", line);

	/*
	 * We take the semaphore inside uart_get to guarantee that we won't
	 * be re-entered while allocating the info structure, or while we
	 * request any IRQs that the driver may need.  This also has the nice
	 * side-effect that it delays the action of uart_hangup, so we can
	 * guarantee that info->tty will always contain something reasonable.
	 */
	state->count++;
	retval = uart_get(state);
	if (retval) {
		state->count--;
		goto fail;
	}

	retval = -ENODEV;

	state->info->tty = tty;

	/*
	 * Make sure the device is in D0 state.
	 */
	if (state->count == 1)	// always true!
		uart_change_pm(state, 0);

	/*
	 * Start up the serial port.
	 */
	retval = uart_startup(state, 0);

	mutex_unlock(&state->mutex);

	/*
	 * If this is the first open to succeed, adjust things to suit.
	 */
	if (retval == 0 && !(state->info->flags & UIF_NORMAL_ACTIVE)) {
		state->info->flags |= UIF_NORMAL_ACTIVE;

		uart_update_termios(state);
	}

 fail:
	return retval;
}

#if 0
static const char *uart_type(struct uart_port *port)
{
	const char *str = NULL;

	if (port->ops->type)
		str = port->ops->type(port);

	if (!str)
		str = "unknown";

	return str;
}

#ifdef CONFIG_SERIAL_CORE_CONSOLE
/*
 *	uart_console_write - write a console message to a serial port
 *	@port: the port to write the message
 *	@s: array of characters
 *	@count: number of characters in string to write
 *	@write: function to write character to port
 */
void uart_console_write(struct uart_port *port, const char *s,
			unsigned int count,
			void (*putchar)(struct uart_port *, int))
{
	unsigned int i;

	for (i = 0; i < count; i++, s++) {
		if (*s == '\n')
			putchar(port, '\r');
		putchar(port, *s);
	}
}
EXPORT_SYMBOL_GPL(uart_console_write);

/*
 *	Check whether an invalid uart number has been specified, and
 *	if so, search for the first available port that does have
 *	console support.
 */
struct uart_port * __init
uart_get_console(struct uart_port *ports, int nr, struct console *co)
{
	int idx = co->index;

	if (idx < 0 || idx >= nr || (ports[idx].iobase == 0 &&
				     ports[idx].membase == NULL))
		for (idx = 0; idx < nr; idx++)
			if (ports[idx].iobase != 0 ||
			    ports[idx].membase != NULL)
				break;

	co->index = idx;

	return ports + idx;
}

/**
 *	uart_parse_options - Parse serial port baud/parity/bits/flow contro.
 *	@options: pointer to option string
 *	@baud: pointer to an 'int' variable for the baud rate.
 *	@parity: pointer to an 'int' variable for the parity.
 *	@bits: pointer to an 'int' variable for the number of data bits.
 *	@flow: pointer to an 'int' variable for the flow control character.
 *
 *	uart_parse_options decodes a string containing the serial console
 *	options.  The format of the string is <baud><parity><bits><flow>,
 *	eg: 115200n8r
 */
void __init
uart_parse_options(char *options, int *baud, int *parity, int *bits, int *flow)
{
	char *s = options;

	*baud = simple_strtoul(s, NULL, 10);
	while (*s >= '0' && *s <= '9')
		s++;
	if (*s)
		*parity = *s++;
	if (*s)
		*bits = *s++ - '0';
	if (*s)
		*flow = *s;
}

struct baud_rates {
	unsigned int rate;
	unsigned int cflag;
};

static const struct baud_rates baud_rates[] = {
	{ 921600, B921600 },
	{ 460800, B460800 },
	{ 230400, B230400 },
	{ 115200, B115200 },
	{  57600, B57600  },
	{  38400, B38400  },
	{  19200, B19200  },
	{   9600, B9600   },
	{   4800, B4800   },
	{   2400, B2400   },
	{   1200, B1200   },
	{      0, B38400  }
};

/**
 *	uart_set_options - setup the serial console parameters
 *	@port: pointer to the serial ports uart_port structure
 *	@co: console pointer
 *	@baud: baud rate
 *	@parity: parity character - 'n' (none), 'o' (odd), 'e' (even)
 *	@bits: number of data bits
 *	@flow: flow control character - 'r' (rts)
 */
int __init
uart_set_options(struct uart_port *port, struct console *co,
		 int baud, int parity, int bits, int flow)
{
	struct ktermios termios;
	int i;

	/*
	 * Ensure that the serial console lock is initialised
	 * early.
	 */
	spin_lock_init(&port->lock);
	lockdep_set_class(&port->lock, &port_lock_key);

	memset(&termios, 0, sizeof(struct ktermios));

	termios.c_cflag = CREAD | HUPCL | CLOCAL;

	/*
	 * Construct a cflag setting.
	 */
	for (i = 0; baud_rates[i].rate; i++)
		if (baud_rates[i].rate <= baud)
			break;

	termios.c_cflag |= baud_rates[i].cflag;

	if (bits == 7)
		termios.c_cflag |= CS7;
	else
		termios.c_cflag |= CS8;

	switch (parity) {
	case 'o': case 'O':
		termios.c_cflag |= PARODD;
		/*fall through*/
	case 'e': case 'E':
		termios.c_cflag |= PARENB;
		break;
	}

	if (flow == 'r')
		termios.c_cflag |= CRTSCTS;

	port->ops->set_termios(port, &termios, NULL);
	co->cflag = termios.c_cflag;

	return 0;
}
#endif /* CONFIG_SERIAL_CORE_CONSOLE */
#endif

static void uart_change_pm(struct uart_state *state, int pm_state)
{
	struct uart_port *port = state->port;

	if (state->pm_state != pm_state) {
		if (port->ops->pm)
			port->ops->pm(port, pm_state, state->pm_state);
		state->pm_state = pm_state;
	}
}

int uart_suspend_port(struct uart_driver *drv, struct uart_port *port)
{
	struct uart_state *state = drv->state + port->line;

	mutex_lock(&state->mutex);

#ifdef CONFIG_DISABLE_CONSOLE_SUSPEND
	if (uart_console(port)) {
		mutex_unlock(&state->mutex);
		return 0;
	}
#endif

	struct uart_info * info = state->info;
	if (info && info->flags & UIF_INITIALIZED) {
		const struct uart_ops *ops = port->ops;

		info->flags = (info->flags & ~UIF_INITIALIZED)
				     | UIF_SUSPENDED;

		spin_lock_irq(&port->lock);
		ops->stop_tx(port);
		ops->set_mctrl(port, 0);
		ops->stop_rx(port);
		spin_unlock_irq(&port->lock);

		/*
		 * Wait for the transmitter to empty.
		 */
		uart_wait_until_sent(info->tty, 0);

		ops->shutdown(port);
	}

	/*
	 * Disable the console device before suspending.
	 */
	if (uart_console(port))
		;

	uart_change_pm(state, 3);

	mutex_unlock(&state->mutex);

	return 0;
}

int uart_resume_port(struct uart_driver *drv, struct uart_port *port)
{
	struct uart_state *state = drv->state + port->line;

	mutex_lock(&state->mutex);

#ifdef CONFIG_DISABLE_CONSOLE_SUSPEND
	if (uart_console(port)) {
		mutex_unlock(&state->mutex);
		return 0;
	}
#endif

	uart_change_pm(state, 0);

	/*
	 * Re-enable the console device after suspending.
	 */
	if (uart_console(port)) {
	}

	if (state->info && state->info->flags & UIF_SUSPENDED) {
		const struct uart_ops *ops = port->ops;
		int ret;

		ops->set_mctrl(port, 0);
		ret = ops->startup(port);
		if (ret == 0) {
			uart_change_speed(state, NULL);
			spin_lock_irq(&port->lock);
			ops->set_mctrl(port, port->mctrl);
			ops->start_tx(port);
			spin_unlock_irq(&port->lock);
			state->info->flags |= UIF_INITIALIZED;
		} else {
			/*
			 * Failed to resume - maybe hardware went away?
			 * Clear the "initialized" flag so we won't try
			 * to call the low level drivers shutdown method.
			 */
			uart_shutdown(state);
		}

		state->info->flags &= ~UIF_SUSPENDED;
	}

	mutex_unlock(&state->mutex);

	return 0;
}

static inline void
uart_report_port(struct uart_driver *drv, struct uart_port *port)
{
  // Does nothing, for now.
}

static void
uart_configure_port(struct uart_driver *drv, struct uart_state *state,
		    struct uart_port *port)
{
	unsigned int flags;

	/*
	 * If there isn't a port here, don't do anything further.
	 */
	if (!port->iobase && !port->mapbase && !port->membase)
		return;

	/*
	 * Now do the auto configuration stuff.  Note that config_port
	 * is expected to claim the resources and map the port for us.
	 */
	flags = UART_CONFIG_TYPE;
	if (port->flags & UPF_AUTO_IRQ)
		flags |= UART_CONFIG_IRQ;
	if (port->flags & UPF_BOOT_AUTOCONF) {
		port->type = PORT_UNKNOWN;
		port->ops->config_port(port, flags);
	}

	if (port->type != PORT_UNKNOWN) {
		unsigned long flags;

		uart_report_port(drv, port);

		/* Power up port for set_mctrl() */
		uart_change_pm(state, 0);

		/*
		 * Ensure that the modem control lines are de-activated.
		 * We probably don't need a spinlock around this, but
		 */
		spin_lock_irqsave(&port->lock, flags);
		port->ops->set_mctrl(port, 0);
		spin_unlock_irqrestore(&port->lock, flags);

		/*
		 * Power down all ports by default, except the
		 * console if we have one.
		 */
		if (!uart_console(port))
			uart_change_pm(state, 3);
	}
}

#if 0
static const struct tty_operations uart_ops = {
	.open		= uart_open,
	.close		= uart_close,
	.write		= uart_write,
	.put_char	= uart_put_char,
	.flush_chars	= uart_flush_chars,//// calls uart_start
	.write_room	= uart_write_room,
	.chars_in_buffer= uart_chars_in_buffer,
	.flush_buffer	= uart_flush_buffer,
	.ioctl		= uart_ioctl,////
	.throttle	= uart_throttle,
	.unthrottle	= uart_unthrottle,
	.send_xchar	= uart_send_xchar,
	.set_termios	= uart_set_termios,////
	.stop		= uart_stop,
	.start		= uart_start,
	.hangup		= uart_hangup,
	.break_ctl	= uart_break_ctl,
	.wait_until_sent= uart_wait_until_sent,
#ifdef CONFIG_PROC_FS
	.read_proc	= uart_read_proc,
#endif
	.tiocmget	= uart_tiocmget,
	.tiocmset	= uart_tiocmset,
};
#endif

/**
 *	uart_register_driver - register a driver with the uart core layer
 *	@drv: low level driver structure
 *
 *	Register a uart driver with the core driver.  We in turn register
 *	with the tty layer, and initialise the core driver per-port state.
 *
 *	drv->port should be NULL, and the per-port structures should be
 *	registered using uart_add_one_port after this call has succeeded.
 */
int uart_register_driver(struct uart_driver *drv)
{
	BUG_ON(drv->state);
	assert(drv->nr == 1);

	struct tty_driver * driver = &theTtyDriver;

	drv->state = &theUartState;
	drv->tty_driver = driver;

	driver->owner		= drv->owner;
	driver->driver_name	= drv->driver_name;
	driver->name		= drv->dev_name;
#if 0
	driver->init_termios.c_iflag = ICRNL | IXON;
	driver->init_termios.c_cflag = BOTHER | CS8 | CREAD | CLOCAL;
	memcpy(&driver->init_termios.c_cc, INIT_C_CC,
               sizeof(driver->init_termios.c_cc));
	driver->init_termios.c_ospeed = 9600;
#endif
	driver->flags		= TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	driver->driver_state    = drv;

	return 0;
}

/**
 *	uart_unregister_driver - remove a driver from the uart core layer
 *	@drv: low level driver structure
 */
void uart_unregister_driver(struct uart_driver *drv)
{
}

#if 0
struct tty_driver *uart_console_device(struct console *co, int *index)
{
	struct uart_driver *p = co->data;
	*index = co->index;
	return p->tty_driver;
}
#endif

/**
 *	uart_add_one_port - attach a driver-defined port structure
 *	@drv: pointer to the uart low level driver structure for this port
 *	@port: uart port structure to use for this port.
 *
 *	This allows the driver to register its own uart_port structure
 *	with the core driver.  The main purpose is to allow the low
 *	level uart drivers to expand uart_port, rather than having yet
 *	more levels of structures.
 */
int uart_add_one_port(struct uart_driver *drv, struct uart_port *port)
{
	struct uart_state *state;
	int ret = 0;

	BUG_ON(in_interrupt());

	if (port->line >= drv->nr)
		return -EINVAL;

	state = drv->state + port->line;

	mutex_lock(&port_mutex);
	mutex_lock(&state->mutex);
	if (state->port) {
		ret = -EINVAL;
		goto out;
	}

	state->port = port;

	port->cons = drv->cons;
	port->info = state->info;

	/*
	 * If this port is a console, then the spinlock is already
	 * initialised.
	 */
	if (!(uart_console(port) && (false))) {
		spin_lock_init(&port->lock);
		lockdep_set_class(&port->lock, &port_lock_key);
	}

	uart_configure_port(drv, state, port);

#if 0
	/*
	 * Register the port whether it's detected or not.  This allows
	 * setserial to be used to alter this ports parameters.
	 */
	tty_register_device(drv->tty_driver, port->line, port->dev);
#endif

#if 0 // no console support
	/*
	 * If this driver supports console, and it hasn't been
	 * successfully registered yet, try to re-register it.
	 * It may be that the port was not available.
	 */
	if (port->type != PORT_UNKNOWN &&
	    port->cons && !(port->cons->flags & CON_ENABLED))
		register_console(port->cons);
#endif

	/*
	 * Ensure UPF_DEAD is not set.
	 */
	port->flags &= ~UPF_DEAD;

 out:
	mutex_unlock(&state->mutex);
	mutex_unlock(&port_mutex);

	return ret;
}

/**
 *	uart_remove_one_port - detach a driver defined port structure
 *	@drv: pointer to the uart low level driver structure for this port
 *	@port: uart port structure for this port
 *
 *	This unhooks (and hangs up) the specified port structure from the
 *	core driver.  No further calls will be made to the low-level code
 *	for this port.
 */
int uart_remove_one_port(struct uart_driver *drv, struct uart_port *port)
{
	struct uart_state *state = drv->state + port->line;
	struct uart_info *info;

	BUG_ON(in_interrupt());

	if (state->port != port)
		printk(KERN_ALERT "Removing wrong port: %p != %p\n",
			state->port, port);

	mutex_lock(&port_mutex);

	/*
	 * Mark the port "dead" - this prevents any opens from
	 * succeeding while we shut down the port.
	 */
	mutex_lock(&state->mutex);
	port->flags |= UPF_DEAD;
	mutex_unlock(&state->mutex);

	info = state->info;
#if 0
	/*
	 * Remove the devices from the tty layer
	 */
	tty_unregister_device(drv->tty_driver, port->line);

	if (info && info->tty)
		tty_vhangup(info->tty);
#endif

	/*
	 * All users of this port should now be disconnected from
	 * this driver, and the port shut down.  We should be the
	 * only thread fiddling with this port from now on.
	 */
	state->info = NULL;

	/*
	 * Free the port IO and memory resources, if any.
	 */
	if (port->type != PORT_UNKNOWN)
		port->ops->release_port(port);

	/*
	 * Indicate that there isn't a port here anymore.
	 */
	port->type = PORT_UNKNOWN;

	/*
	 * free resources.
	 */
	if (info) {
		kfree(info);
	}

	state->port = NULL;
	mutex_unlock(&port_mutex);

	return 0;
}

#if 0
/*
 *	Are the two ports equivalent?
 */
int uart_match_port(struct uart_port *port1, struct uart_port *port2)
{
	if (port1->iotype != port2->iotype)
		return 0;

	switch (port1->iotype) {
	case UPIO_PORT:
		return (port1->iobase == port2->iobase);
	case UPIO_HUB6:
		return (port1->iobase == port2->iobase) &&
		       (port1->hub6   == port2->hub6);
	case UPIO_MEM:
	case UPIO_MEM32:
	case UPIO_AU:
	case UPIO_TSI:
	case UPIO_DWAPB:
		return (port1->mapbase == port2->mapbase);
	}
	return 0;
}
#endif

/* There are two input buffers, each of size inputBufSize.
   One may be in use sending to the receiver. 
   The input buffers hold pairs of (flag, character). */

#define inputBufSize UART_XMIT_SIZE	// number of pairs
char inputBuf0[inputBufSize*2];
char inputBuf1[inputBufSize*2];

/* inputBufSending is 0 if we are currently sending to the receiver
from buffer 0, 1 if from 1, 2 if not currently sending. */
unsigned int inputBufSending = 2;

/* inputBufUsed[i] is the number of pairs of data in each buffer
   (including data that has already been sent to the receiver).
   inputBufUsed[2] is a dummy to simplify use of inputBufSending. */
unsigned int inputBufUsed[3];	// number of pairs in each buffer

/* inputBufRead is the number of pairs already read from
   inputBuf[inputBufForOutput]. */
unsigned int inputBufRead = 0;

char * const inputBuf[2] = {inputBuf0, inputBuf1} ;
unsigned int inputBufForInput = 0;	// 0 or 1
unsigned int inputBufForOutput = 0;	// 0 or 1

capKludge readWaiter;
bool readInUse = false;;
unsigned long readWaiterCount;

// Stuff from tty_io.h.

// Called with port lock held.
// Returns 1 if copied, 0 if not.
int
tty_insert_flip_char(struct tty_struct * tty,
                     unsigned char ch, char flag)
{
  unsigned int remainingRoom = inputBufSize - inputBufUsed[inputBufForInput];
  if (remainingRoom <= TTY_THRESHOLD_THROTTLE) {
    if (remainingRoom == 0) {
      // The current input buffer is full.
      unsigned int otherBuf = inputBufForInput ^ 1;
      if (inputBufUsed[otherBuf] != 0)
        return 0;	// too bad, lose the data
			// (and we will have throttled long before this point)
      inputBufForInput = otherBuf;	// switch input buffers
    } else {
      uart_throttle(tty);
    }
  }
  char * p = &inputBuf[inputBufForInput][inputBufUsed[inputBufForInput]++ * 2];
  *p++ = flag;
  *p = ch;
  return 1;
}

void
tty_flip_buffer_push(struct tty_struct * tty)
{
  struct uart_state * state = tty->driver_data;
  struct uart_port * port = state->port;
  unsigned long flags;
  spin_lock_irqsave(&port->lock, flags);

  if (readInUse) {		// there is a reader waiting
    unsigned int left = inputBufUsed[inputBufForOutput] - inputBufRead;
    if (left > 0) {
      // there is data available now
      unsigned int readCount = readWaiterCount;
      if (readCount > left)
        readCount = left;	// take min

      capros_Node_getSlotExtended(KR_KEYSTORE, toCap(readWaiter), KR_TEMP0);
      Message msg;
      msg.snd_key0 = msg.snd_key1 = msg.snd_key2 = msg.snd_rsmkey = KR_VOID;
      msg.snd_w1 = msg.snd_w2 = msg.snd_w3 = 0;
      msg.snd_code = RC_OK;
      msg.snd_data = &inputBuf[inputBufForOutput][inputBufRead * 2];
      msg.snd_len = readCount * 2;
      msg.snd_invKey = KR_TEMP0;
      PSEND(&msg);	// prompt send

      readInUse = false;

      // Did we take all the data in this buffer?
      if (readCount == left) {	// yes
        /* Ensure input now goes to the other buffer.
           Output will come from the other buffer next time. */
        inputBufForInput = inputBufForOutput ^= 1;
        inputBufRead = 0;
        uart_unthrottle(tty);
      } else {
        inputBufRead += readCount;
      }
        
    } // else no data now
  }
  spin_unlock_irqrestore(&port->lock, flags);
}

// Stuff from arch/arm/mach-ep93xx/core.c
#define EP93XX_UART_MCR_OFFSET		(0x0100)

static void ep93xx_uart_set_mctrl(struct amba_device *dev,
				  void __iomem *base, unsigned int mctrl)
{
	unsigned int mcr;

	mcr = 0;
	if (!(mctrl & TIOCM_RTS))
		mcr |= 2;
	if (!(mctrl & TIOCM_DTR))
		mcr |= 1;

	__raw_writel(mcr, base + EP93XX_UART_MCR_OFFSET);
}

static void ep93xx_uart_gate_clk(bool enable, uint32_t mask)
{
  capros_Node_getSlotExtended(KR_LINUX_EMUL, LE_DEVPRIVS, KR_TEMP0);
  result_t result = capros_DevPrivs_deviceConfig(KR_TEMP0, enable, mask);
  assert(result == RC_OK);	// else misconfiguration
}

#if 0 // not used yet

static void ep93xx_uart_gate_clk1(bool enable)
{
  ep93xx_uart_gate_clk(enable, SYSCONDeviceCfg_U1EN);
}

static struct amba_pl010_data ep93xx_uart_data1 = {
	.set_mctrl = ep93xx_uart_set_mctrl,
	.gate_clk  = ep93xx_uart_gate_clk1
};

static struct amba_device uart1_device = {
	.dev		= {
		.bus_id		= "apb:uart1",
		.platform_data	= &ep93xx_uart_data1,
	},
	.res		= {
		.start	= EP93XX_UART1_PHYS_BASE,
		.end	= EP93XX_UART1_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART1, NO_IRQ },
	.periphid	= 0x00041010,
};

static void ep93xx_uart_gate_clk2(bool enable)
{
  ep93xx_uart_gate_clk(enable, SYSCONDeviceCfg_U2EN);
}

static struct amba_pl010_data ep93xx_uart_data2 = {
	.set_mctrl = 0,
	.gate_clk  = ep93xx_uart_gate_clk2
};

static struct amba_device uart2_device = {
	.dev		= {
		.bus_id		= "apb:uart2",
		.platform_data	= &ep93xx_uart_data2,
	},
	.res		= {
		.start	= EP93XX_UART2_PHYS_BASE,
		.end	= EP93XX_UART2_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART2, NO_IRQ },
	.periphid	= 0x00041010,
};
#endif

static void ep93xx_uart_gate_clk3(bool enable)
{
  ep93xx_uart_gate_clk(enable, SYSCONDeviceCfg_U3EN);
}

static struct amba_pl010_data ep93xx_uart_data3 = {
	.set_mctrl = ep93xx_uart_set_mctrl,
	.gate_clk  = ep93xx_uart_gate_clk3
};

static struct amba_device uart3_device = {
	.dev		= {
		.bus_id		= "apb:uart3",
		.platform_data	= &ep93xx_uart_data3,
	},
	.res		= {
		.start	= EP93XX_UART3_PHYS_BASE,
		.end	= EP93XX_UART3_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART3, NO_IRQ },
	.periphid	= 0x00041010,
};

int
amba_driver_register(struct amba_driver * drv)
{
  /* For now, bypass all the bus stuff and just get the serial port working. */
  int ret = (*drv->probe)(&uart3_device, 0 /* id not used by amba-pl010 */);
  if (ret)
    printk(KERN_ERR "uart3 probe returned %d!\n", ret);
  return 0;
}

void amba_driver_unregister(struct amba_driver * drv)
{
  printk(KERN_ERR "amba_driver_unregister called.\n");	// FIXME
}

int
main(void)
{
  result_t result;

  Message msgs;
  Message * const msg = &msgs;	// to address it consistently

  /* Validate that SIZEOF_THREAD_INFO is correct, or at least good enough. */
  assert(SIZEOF_THREAD_INFO
         >= offsetof(struct thread_info, preempt_count)
            + sizeof(((struct thread_info *)0)->preempt_count) );

  // Create the KEYSTORE object.
  result = capros_Constructor_request(KR_KEYSTORE, KR_BANK, KR_SCHED, KR_VOID,
                             KR_KEYSTORE);
  assert(result == RC_OK);	// FIXME
  result = capros_SuperNode_allocateRange(KR_KEYSTORE, LKSN_THREAD_PROCESS_KEYS,
                      LKSN_MAPS_GPT);
  assert(result == RC_OK);	// FIXME
  // Populate it.
  capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_THREAD_PROCESS_KEYS+0,
                               KR_SELF, KR_VOID);
  capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_STACKS_GPT,
                               KR_RETURN, KR_VOID);

  // Create the lsync process.
  unsigned int lsyncThreadNum;
  result = lthread_new_thread(LSYNC_STACK_SIZE, &lsync_main, NULL,
                              &lsyncThreadNum);
  if (result != RC_OK) {
    assert(false);	// FIXME handle error
  }

  // Get lsync process key
  result = capros_Node_getSlotExtended(KR_KEYSTORE,
                              LKSN_THREAD_PROCESS_KEYS + lsyncThreadNum,
                              KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Process_makeStartKey(KR_TEMP0, 0, KR_LSYNC);
  assert(result == RC_OK);
  result = capros_Process_swapKeyReg(KR_TEMP0, KR_LSYNC, KR_LSYNC, KR_VOID);
  assert(result == RC_OK);
  

  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          xmitWaiterCap, xmitWaiterCap);
  if (result != RC_OK) {
    assert(false);	// FIXME handle error
  }

  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
             toCap(wmdGlobal.waiter), toCap(wmdGlobal.waiter));
  if (result != RC_OK) {
    assert(false);	// FIXME handle error
  }
  wmdGlobal.inUse = false;

  // FIXME allocate all these contiguously to save space
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
             toCap(readWaiter), toCap(readWaiter));
  if (result != RC_OK) {
    assert(false);	// FIXME handle error
  }

  extern int __init pl010_init(void);
  if (pl010_init()) {	// FIXME: need to make this configurable
    assert(false);	// FIXME handle error
  }


  struct tty_struct * tty = &theTtyStruct;
  struct uart_state * state = &theUartState;

  msg->rcv_key0 = msg->rcv_key1 = msg->rcv_key2 = KR_VOID;
  msg->rcv_rsmkey = KR_RETURN;

  msg->snd_invKey = KR_VOID;
  msg->snd_len = 0;
  /* The void key is not picky about the other parameters,
     so it's OK to leave them uninitialized. */

  for (;;) {
    msg->rcv_data = msgRcvBuf;
    msg->rcv_limit = msgRcvBufSize;
    assert(msg->rcv_limit > sizeof(struct capros_SerialPort_icounter));
    RETURN(msg);
kdprintf(KR_OSTREAM, "Called, oc=0x%x\n", msg->rcv_code);////

    /* If we were sending from an input buffer, we no longer are. */
    if (inputBufSending != 2) {
      inputBufUsed[inputBufSending] = 0;
      inputBufSending = 2;
      uart_unthrottle(tty);
    }

    // Set defaults for return.
    msg->snd_invKey = KR_RETURN;
    msg->snd_code = RC_OK;
    msg->snd_key0 = msg->snd_key1 = msg->snd_key2 = msg->snd_rsmkey = KR_VOID;
    msg->snd_w1 = msg->snd_w2 = msg->snd_w3 = 0;
    msg->snd_len = 0;

    switch (msg->rcv_code) {
    case OC_capros_key_getType:
      msg->snd_w1 = IKT_capros_SerialPort;
      break;

    case OC_capros_SerialPort_open:
    {
      if (state->count) {	// already open - can only open once
        msg->snd_code = RC_capros_SerialPort_Already;
        break;
      }
      int ret = uart_open(tty);
      msg->snd_w1 = ret;
      if (ret) msg->snd_code = RC_capros_key_NoAccess;

      break;
    }

    case OC_capros_SerialPort_close:
      if (state->count != 1) {	// already closed
        msg->snd_code = RC_capros_SerialPort_Already;
        break;
      }
      uart_close(tty);
      break;

    case 0:	// read (not yet working in IDL)
    {
      struct uart_port * port = state->port;
      unsigned long flags;
      unsigned long readCount = msg->snd_w1;	// max number of pairs to read
      if (! port		// port isn't open
          || readCount <= 0) {
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      /* A large readCount is not a problem. We will not send more data
         than is in an inputBuf. */

      spin_lock_irqsave(&port->lock, flags);
      unsigned int left = inputBufUsed[inputBufForOutput] - inputBufRead;
      if (left > 0) {
        // there is data available now
        if (readCount > left)
          readCount = left;	// take min

        msg->snd_len = readCount * 2;
        msg->snd_data = &inputBuf[inputBufForOutput][inputBufRead * 2];

        // Are we taking all the data in this buffer?
        if (readCount == left) {	// yes
          /* Leave inputBufUsed[inputBufForOutput] nonzero to prevent it from
          being overwritten before the data is sent.
          It will be zeroed the next time we receive a call. */
          inputBufSending = inputBufForOutput;
          /* Ensure input now goes to the other buffer.
             Output will come from the other buffer next time. */
          inputBufForInput = (inputBufForOutput ^= 1);
          inputBufRead = 0;
        } else {
          inputBufRead += readCount;
        }
        
        spin_unlock_irqrestore(&port->lock, flags);
      } else {		// must wait
        // Set up to wait.
        readInUse = true;
        readWaiterCount = readCount;
        capros_Node_swapSlotExtended(KR_KEYSTORE, toCap(readWaiter),
                               KR_RETURN, KR_VOID);
        spin_unlock_irqrestore(&port->lock, flags);

        // The resume key to the caller has been saved.
        msg->snd_invKey = KR_VOID;
      }

      break;
    }

    case 1: ;	// write (not yet working in IDL)
      /* No buffering of transmitted data, pending implementation
      of the Indirect Data capability. */
        // FIXME: fix all this when msgRcvBuf issue is resolved
        int x = uart_write(tty, msgRcvBuf, msg->rcv_sent);
        (void)x;	// FIXME
      break;

    case OC_capros_SerialPort_setTermios2:
    {
      if (msg->rcv_sent < sizeof(struct capros_SerialPort_termios2)) {
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      struct ktermios old_termios = *tty->termios;
      /* struct capros_SerialPort_termios2 has the same structure as
         struct ktermios. */
      *tty->termios = *(struct ktermios *) msg->rcv_data;
      uart_set_termios(tty, &old_termios);
      break;
    }

    case OC_capros_SerialPort_waitForBreak:

      if (brkGlobal.inUse) {
        msg->snd_code = RC_capros_SerialPort_already;
      } else {
        // Set up to wait.
        brkGlobal.inUse = true;
        capros_Node_swapSlotExtended(KR_KEYSTORE, toCap(brkGlobal.waiter),
                               KR_RETURN, KR_VOID);

        // The resume key to the caller has been saved.
        msg->snd_invKey = KR_VOID;
      }
      break;

    case OC_capros_SerialPort_waitForICountChange:
      if (msg->rcv_sent < sizeof(struct capros_SerialPort_icounter)) {
        msg->snd_code = RC_capros_key_RequestError;
      } else {
        int ret = uart_wait_modem_status(state,
                    (struct capros_SerialPort_icounter *)msgRcvBuf,
                    (struct capros_SerialPort_icounter *)msgRcvBuf,
                    msg->rcv_w1);
        switch (ret) {
        case 0:		// the caller waits
          // The resume key to the caller has been saved.
          msg->snd_invKey = KR_VOID;
          break;

        case 1:		// we return immediately
          msg->snd_len = sizeof(struct capros_SerialPort_icounter);
          msg->snd_data = msgRcvBuf;
          break;

        case 2:		// a process is already waiting
          msg->snd_code = RC_capros_SerialPort_already;
          break;
        }         
      }
      break;

    case OC_capros_SerialPort_getICounts:
      uart_get_count(state, (struct capros_SerialPort_icounter *)msgRcvBuf);
      msg->snd_data = msgRcvBuf;
      msg->snd_len = sizeof(struct capros_SerialPort_icounter);
      break;

    case OC_capros_SerialPort_discardBufferedOutput:
      uart_flush_buffer(tty);
      break;

    case OC_capros_SerialPort_writeHighPriorityChar:
      uart_send_xchar(tty, msg->rcv_w1);
      break;

    case OC_capros_SerialPort_stopTransmission:
      uart_stop(tty);
      break;

    case OC_capros_SerialPort_startTransmission:
      uart_start(tty);
      break;

    case OC_capros_SerialPort_beginBreak:
      uart_break_ctl(tty, -1);
      break;

    case OC_capros_SerialPort_endBreak:
      uart_break_ctl(tty, 0);
      break;

    case OC_capros_SerialPort_waitUntilSent:
      uart_wait_until_sent(tty, 0);
      break;

    case OC_capros_SerialPort_getModemStatus:
      msg->snd_w1 = uart_tiocmget(tty, 0);
      break;

    case OC_capros_SerialPort_setModemStatus:
      uart_tiocmset(tty, 0, msg->rcv_w1, msg->rcv_w2);
      break;

    }
  }
  return 0;
}

//// FIXME
unsigned long msleep_interruptible(unsigned int msecs)
{
  assert(false);	// need to implement
  return 0;		// no signals in CapROS
}
