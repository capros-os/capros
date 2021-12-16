#ifndef __EVENTQUEUEKEY_H__
#define __EVENTQUEUEKEY_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

#define OC_EvtQ_Post                        1
#define OC_EvtQ_Poll                        2
#define OC_EvtQ_Wait                        3

#define RC_EvtQ_Full                        1
#define RC_EvtQ_Empty                       2

#ifndef __ASSEMBLER__

typedef struct event_s {
  uint32_t w[3];
} event_s;

uint32_t evtq_post(uint32_t krEvtQ, event_s *evt);
uint32_t evtq_poll(uint32_t krEvtQ, /* out */ event_s *evt);
uint32_t evtq_wait(uint32_t krEvtQ, /* out */ event_s *evt);

#endif


#endif /* __EVENTQUEUEKEY_H__ */

