/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
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

#ifndef __net_h__
#define __net_h__

/* Process Requestor codes */
#define OC_netif_open    12    /* Open the network card for use */
#define OC_netif_receive 13    /* Receive data from the card */
#define OC_netif_xmit    14    /* Transmit data */
#define OC_netif_close   15    /* Close netif */
#define OC_netif_mode    16    /* Set the operating params for the network */


/*helper calling codes*/
#define OC_irq_arrived        11  /* Helper received an interrupt */
#define OC_netdriver_key      12  /* Give helper start key to the core driver
				   * process */


/* Error Codes */
#define RC_PCI_PROBE_ERROR     99   /* Error during pci probe */
#define RC_NETIF_INIT_FAILED   98
#define RC_NETIF_OPEN_FAILED   97
#define RC_IRQ_ALLOC_FAILED    96
#define RC_IRQ_RELEASE_FAILED  95
#define RC_HELPER_START_FAILED 94   /* Unable to start the helper */

#endif /*__net_h__*/
