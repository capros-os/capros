/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
