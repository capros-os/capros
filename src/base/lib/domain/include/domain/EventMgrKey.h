#ifndef __EVENTMGRKEY_H__
#define __EVENTMGRKEY_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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

/* This file defines the API for the Event Manager domain
   interface. This domain's purpose is to dispatch input events from
   mouse, keyboard, etc., to a consumer domain (usually the window system). 
*/

#define OC_EventMgr_KeyData       1
#define OC_EventMgr_MouseData     2

/* stubs */
uint32_t event_mgr_queue_key_data(cap_t kr_eventmgr, uint32_t data);
uint32_t event_mgr_queue_mouse_data(cap_t kr_eventmgr, int32_t mask,
				    int8_t xmotion, int8_t ymotion);

#endif
