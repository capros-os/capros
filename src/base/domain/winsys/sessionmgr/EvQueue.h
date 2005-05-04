#ifndef __EVQUEUE_H__
#define __EVQUEUE_H__
/*
 * Copyright (C) 2003, Jonathan Shapiro.
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <domain/SessionKey.h>

#define MAXEVENTS 300
typedef struct EvQueue EvQueue;
struct EvQueue {
  Event list[MAXEVENTS];
  int32_t head;
  int32_t tail;
};

/* Define the interface for the window system to insert/retrieve
   events.  This is *not* available to winsys clients.  */
void EvQueue_Clear(EvQueue *evq);

bool EvQueue_Insert(EvQueue *evq, Event ev);

bool EvQueue_Remove(EvQueue *evq,
		 /* out */ Event *ev);

bool EvQueue_IsEmpty(EvQueue *evq);

#endif
