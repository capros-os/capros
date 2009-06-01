#ifndef __SESSION_KEY_H__
#define __SESSION_KEY_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Mouse button masks */
#define MOUSE_LEFT   0x1u
#define MOUSE_RIGHT  0x2u
#define MOUSE_MIDDLE 0x4u

enum EventType {
  Null,
  Mouse,
  Key,
  Resize,
  DragOver,
};

typedef struct Event Event;
typedef struct MouseEvent MouseEvent;
typedef struct KeyEvent KeyEvent;
typedef struct DragOverEvent DragOverEvent;

/* This structure provides one class for all possible events. A server
   uses this class to queue all events */
struct Event {
  enum EventType type;
  uint32_t window_id;		/* Events are dispatched to
				   sessions. The session uses this
				   field to dispatch the event to a
				   particular window on the client
				   side. */
  bool processed;
  uint32_t data[5];
};

/* FIX: may need to have window system distinguish button down, drag
   and button up events. */
struct MouseEvent {
  bool processed;
  uint32_t window_id;
  uint32_t button_mask;
  uint32_t cursor_x;
  uint32_t cursor_y;
};

struct KeyEvent {
  bool processed;
  uint32_t window_id;
  uint32_t scancode;
};

struct DragOverEvent {
  bool processed;
  uint32_t window_id;
  uint32_t cursor_x;
  uint32_t cursor_y;
};

/* Conversion macros: */
#define IS_MOUSE_EVENT(Event)  (Event.type == Mouse)
#define IS_KEY_EVENT(Event)    (Event.type == Key)
#define IS_RESIZE_EVENT(Event) (Event.type == Resize)
#define IS_DRAGOVER_EVENT(Event) (Event.type == DragOver)

/* Temporary convenience macros until we have a better event queue */
#define EVENT_BUTTON_MASK(ev) (ev.data[0])
#define EVENT_CURSOR_X(ev) (ev.data[1])
#define EVENT_CURSOR_Y(ev) (ev.data[2])
#define EVENT_SCANCODE(ev) (ev.data[0])
#define EVENT_NEW_ORIG_X(ev) (ev.data[0])
#define EVENT_NEW_ORIG_Y(ev) (ev.data[1])
#define EVENT_NEW_WIDTH(ev) (ev.data[2])
#define EVENT_NEW_HEIGHT(ev) (ev.data[3])

/* Client commands */

/* Notes on events: A client will make one invocation to wait on *any*
   events.  The window system domain must determine which events to
   dispatch to the client.  (E.g. A client only gets device events
   (like mouse clicks) when that client has focus.)  When the client
   issues this invocation, if there are no events the window system
   issues a RETRY invocation on the client's resume key, forwarding
   the client to the window system's single wrapper node.  When the
   next event occurs, the window system can selectively wake the
   appropriate clients by telling its wrapper node to wake only those
   clients matching an id (WTR). Those clients that are awakened
   proceed with their *original* invocations, retrieving the new event.
   Furthermore, note that the session key is used to wait for events,
   since events are only dispatched to sessions.  A previous design of
   the window system dispatched events to individual windows, but
   that's a bad decision because it forces a client to wait on
   potentially large numbers of windows. */

/* Window decorations */
#define WINDEC_SHADOW   0x0001u
#define WINDEC_BORDER   0x0002u
#define WINDEC_TITLEBAR 0x0004u
#define WINDEC_RESIZE   0x0008u

#define  OC_Session_NewWindow         400 

#define  OC_Session_Close             401

#define OC_Session_NewSubsession      402
/* Purpose:  Creating subsessions is conceptually how a client would
create a "graphics shell" and allow another domain to render inside
that shell.  Stay tuned for details on this... 
*/


#define OC_Session_NextEvent          403
/* Purpose:  Retrieve the next event from the window system.  Note all
events are dispatched to sessions.  If there are no events, the client
will block on this invocation.
*/


/****** Commands for individual Windows *****/
#define OC_Session_WinMap             404
#define OC_Session_WinUnmap           405
#define OC_Session_WinGetSize         406
#define OC_Session_WinSetClipRegion   407
#define OC_Session_WinKill            408
#define OC_Session_WinSetTitle        409
#define OC_Session_WinRedraw          410
#define OC_Session_WinResize          411
/* Drag and Drop Support */
#define OC_Session_WinDragAndDrop     418

#define OC_Session_NewDefaultWindow   412

#define OC_Session_DisplaySize        413

/* Invisible windows serve as new session containers */
#define OC_Session_NewInvisibleWindow  414

/* Sub-session creation */
#define OC_Session_NewSessionCreator   415

/* Cut/Copy support */
#define OC_Session_PutPasteBuffer      416

/* Paste support */
#define OC_Session_GetPasteBuffer      417

/* Error codes */
#define RC_Session_Retry     680

/* If user wants default parent for any new window, use the following */
#define DEFAULT_PARENT 0

/* Stubs for invocations */
uint32_t session_new_default_window(cap_t kr_session,
				    cap_t kr_client_bank,
				    uint32_t parent_id,
				    /* out */ uint32_t *window_id,
				    cap_t kr_space);

uint32_t session_new_window(cap_t kr_session,
			    cap_t kr_client_bank,
			    uint32_t parent_id,
			    uint32_t parent_location_x,
			    uint32_t parent_location_y,
			    uint32_t width,
			    uint32_t height,
			    uint32_t decorations,
			    /* out */ uint32_t *window_id,
			    cap_t kr_space);

uint32_t session_new_invisible_window(cap_t kr_session,
				      cap_t kr_client_bank,
				      uint32_t parent_id,
				      uint32_t parent_location_x,
				      uint32_t parent_location_y,
				      uint32_t width,
				      uint32_t height,
				      uint32_t qualifier,
				      /* out */ uint32_t *window_id);

uint32_t session_next_event(cap_t kr_session,
			    /* out */ Event *event);

uint32_t session_close(cap_t kr_session);

/* Commands on individual windows */
uint32_t session_win_size(cap_t kr_session, uint32_t window_id,
			  /* out */ uint32_t *width, 
			  /* out */ uint32_t *height);

uint32_t session_win_map(cap_t kr_session, uint32_t window_id);

uint32_t session_win_unmap(cap_t kr_session, uint32_t window_id);

uint32_t session_win_kill(cap_t kr_session, uint32_t window_id);

uint32_t session_win_set_clip_region(cap_t kr_session, uint32_t window_id,
				     uint32_t topLeftX, uint32_t topLeftY,
				     uint32_t bottomRightX, 
				     uint32_t bottomRightY);

uint32_t session_win_set_title(cap_t kr_session, uint32_t window_id,
			       char * title);

uint32_t session_win_redraw(cap_t kr_session,
			    uint32_t window_id,
			    uint32_t topLeftX,
			    uint32_t topLeftY,
			    uint32_t bottomRightX,
			    uint32_t bottomRightY);

uint32_t session_win_resize(cap_t kr_session, uint32_t window_id, 
			    uint32_t width, uint32_t height);

/* Notify winsys that user is initiating drag-n-drop sequence */
uint32_t session_win_drag_and_drop(cap_t kr_session, uint32_t window_id); 

uint32_t session_container_size(cap_t kr_session, 
			      /* out */ uint32_t *width,
			      /* out */ uint32_t *height);

uint32_t session_new_session_creator(cap_t kr_session,
				     cap_t kr_client_bank,
				     uint32_t container_window_id,
				     cap_t kr_new_creator);

uint32_t session_put_pastebuffer(cap_t kr_session,
				 cap_t kr_content,
				 cap_t kr_converter);

uint32_t session_get_pastebuffer(cap_t kr_session,
				 cap_t kr_content,
				 cap_t kr_converter);

#endif
