/*
 *  Copyright 1999 ARM Limited
 *  Copyright (C) 2000-2001 Deep Blue Solutions Ltd.
 * Portions Copyright (C) 2007, Strawberry Development Group.
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

//#include <eros/Invoke.h>	// get RC_OK
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
//#include <linux/interrupt.h>
//#include <linux/err.h>
#include <linux/serial_core.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <asm-generic/semaphore.h>
#include <asm/termios.h>
#include <domain/assert.h>

/*
 * This is used to lock changes in serial line configuration.
 */
static DEFINE_MUTEX(port_mutex);

/*
 * lockdep: port->lock is initialized in two places, but we
 *          want only one lock-class:
 */
static struct lock_class_key port_lock_key;

#define uart_console(port)	(0)

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
		 * Turn off DTR and RTS early.
		 */
		if (!info->tty || (info->tty->termios->c_cflag & HUPCL))
			uart_clear_mctrl(port, TIOCM_DTR | TIOCM_RTS);

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
	 * kill off our tasklet
	 */
	tasklet_kill(&info->tlet);

	/*
	 * Free the transmit buffer page.
	 */
	if (info->xmit.buf) {
		free_page((unsigned long)info->xmit.buf);
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
	unsigned int try, baud, altbaud = 38400;
	upf_t flags = port->flags & UPF_SPD_MASK;

	if (flags == UPF_SPD_HI)
		altbaud = 57600;
	if (flags == UPF_SPD_VHI)
		altbaud = 115200;
	if (flags == UPF_SPD_SHI)
		altbaud = 230400;
	if (flags == UPF_SPD_WARP)
		altbaud = 460800;

	for (try = 0; try < 2; try++) {
		baud = tty_termios_baud_rate(termios);

		/*
		 * The spd_hi, spd_vhi, spd_shi, spd_warp kludge...
		 * Die! Die! Die!
		 */
		if (baud == 38400)
			baud = altbaud;

		/*
		 * Special case: B0 rate.
		 */
		if (baud == 0)
			baud = 9600;

		if (baud >= min && baud <= max)
			return baud;

		/*
		 * Oops, the quotient was zero.  Try again with
		 * the old baud rate if possible.
		 */
		termios->c_cflag &= ~CBAUD;
		if (old) {
			termios->c_cflag |= old->c_cflag & CBAUD;
			old = NULL;
			continue;
		}

		/*
		 * As a last resort, if the quotient is zero,
		 * default to 9600 bps
		 */
		termios->c_cflag |= B9600;
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

	if (termios->c_cflag & CLOCAL)
		state->info->flags &= ~UIF_CHECK_CD;
	else
		state->info->flags |= UIF_CHECK_CD;

	port->ops->set_termios(port, termios, old_termios);
}

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

	if (state->info && state->info->flags & UIF_INITIALIZED) {
		const struct uart_ops *ops = port->ops;

		state->info->flags = (state->info->flags & ~UIF_INITIALIZED)
				     | UIF_SUSPENDED;

		spin_lock_irq(&port->lock);
		ops->stop_tx(port);
		ops->set_mctrl(port, 0);
		ops->stop_rx(port);
		spin_unlock_irq(&port->lock);

		/*
		 * Wait for the transmitter to empty.
		 */
		while (!ops->tx_empty(port)) {
			msleep(10);
		}

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

	/*
	 * Register the port whether it's detected or not.  This allows
	 * setserial to be used to alter this ports parameters.
	 */
	tty_register_device(drv->tty_driver, port->line, port->dev);

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

	/*
	 * Remove the devices from the tty layer
	 */
	tty_unregister_device(drv->tty_driver, port->line);

	info = state->info;
	if (info && info->tty)
		tty_vhangup(info->tty);

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
	 * Kill the tasklet, and free resources.
	 */
	if (info) {
		tasklet_kill(&info->tlet);
		kfree(info);
	}

	state->port = NULL;
	mutex_unlock(&port_mutex);

	return 0;
}

