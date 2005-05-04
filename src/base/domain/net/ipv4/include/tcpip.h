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
                                                                                               
/* Transmission Control Protocol: based on RFC 793 */
                                                                                               

#ifndef __TCPIP_H__
#define __TCPIP_H__

#include "pbuf.h"
/* FIX : Finalise the path to include the files from
#include "api_msg.h"
*/
void tcpip_init(void (* tcpip_init_done)(void *), void *arg);
void tcpip_apimsg(struct api_msg *apimsg);
err_t tcpip_input(struct pbuf *p, struct netif *inp);
err_t tcpip_callback(void (*f)(void *ctx), void *ctx);

void tcpip_tcp_timer_needed(void);

enum tcpip_msg_type {
  TCPIP_MSG_API,
  TCPIP_MSG_INPUT,
  TCPIP_MSG_CALLBACK
};

struct tcpip_msg {
  enum tcpip_msg_type type;
  sys_sem_t *sem;
  union {
    struct api_msg *apimsg;
    struct {
      struct pbuf *p;
      struct netif *netif;
    } inp;
    struct {
      void (*f)(void *ctx);
      void *ctx;
    } cb;
  } msg;
};


#endif /* __TCPIP_H__ */
