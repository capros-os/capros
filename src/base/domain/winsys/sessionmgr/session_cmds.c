/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <stddef.h>
#include <stdlib.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/SessionKey.h>
#include <domain/ConstructorKey.h>

#include <xsprintf.h>

#include "SessionRequest.h"
#include "window_ids.h"
#include "../debug.h"
#include "../winsyskeys.h"
#include "../xclipping.h"
#include "../coordxforms.h"
#include "../fbmgr/fbm_commands.h"
#include "../global.h"
#include "../Decoration.h"
#include "../Invisible.h"
#include <forwarder.h>

/* This is the window system's version of the server-side SessionKey
   implementation.  It will necessarily be different from other
   implementations! 
*/
extern Window *Root;
extern Window *focus;
extern void winsys_change_focus(Window *from, Window *to);

/* Generate a unique id for the given Window object.  Store the id in
the Window object, add the id to the Session's list of windows and
return the new id. */
static uint32_t
session_bind_window(Session *s, Window *w)
{
  uint32_t newid = winid_new(s, w);

  w->window_id = newid;

  DEBUG(session_cmds) 
    kprintf(KR_OSTREAM, "session_bind_window(): checking sess 0x%08x"
	  " [0x%08x]", ADDRESS(s), ADDRESS(w->session));

  return newid;
}

uint32_t
session_NewWindow(Session *s, uint32_t client_bank_slot, 
		  Window *parent, point_t origin,
		  point_t size, uint32_t decs,
		  /* out */ uint32_t *winid,
		  uint32_t window_addrspace)
{
  Decoration *decoration = NULL;
  Window *window = NULL;
  uint8_t title[100];
  uint32_t buffer_id = 0;
  uint32_t result;

  DEBUG(session_cmds) 
    kprintf(KR_OSTREAM, "Inside session_NewWindow()...");

  if (s == NULL)
    return RC_capros_key_RequestError;

  /* First create decoration window, if needed */
  if (decs) {
    decoration = decoration_create(parent, origin, size, s);

    /* Because client request for this window creation doesn't know
    about the decoration window, we need to convert the requested
    origin into the coords of the decoration window. */
    origin.x -= decoration->win.origin.x;
    origin.y -= decoration->win.origin.y;
    window = client_create(&(decoration->win), origin, size, s);
  }
  else
    window = client_create(parent, origin, size, s);

  /* Hand Window object to Session and have Session return a unique
     window id that we can pass back to client */
  *winid = session_bind_window(s, window);

  /* Put a default title in the window (mainly for debugging) */
  xsprintf(title, "Window Id 0x%08x", window->window_id);
  window_set_name(&(decoration->win), title);
  DEBUG(titlebar) 
    kprintf(KR_OSTREAM, "** session_NewWindow():\njust set name to <len=%u> "
	   "[%s]\n         <len=%u> (%s) )",
	  strlen(decoration->win.name), decoration->win.name, strlen(title), 
	    title);

  /* Paranoia check */
  if (s->windows == TREE_NIL)
    kdprintf(KR_OSTREAM, " Uh-oh!");

  /* Reserve (and map) space in the framebuffer for this new client
     window */

  result = fbm_MapClient(KR_CLIENT_BANK, window_addrspace, &buffer_id);
  if (result == RC_OK)
    window->buffer_id = buffer_id;

  return result;
}

uint32_t
session_NewInvisibleWindow(Session *s, Window *parent,
			   point_t origin, point_t size, 
			   uint32_t qualifier,
			   /* out */ uint32_t *winid)
{
  Invisible *inv = NULL;
  uint32_t result = RC_OK;

  DEBUG(session_cmds) 
    kprintf(KR_OSTREAM, "Inside session_NewInvisibleWindow()...");

  if (s == NULL)
    return RC_capros_key_RequestError;

  inv = invisible_create(parent, origin, size, s, qualifier);

  /* Hand Window object to Session and have Session return a unique
     window id that we can pass back to client */
  *winid = session_bind_window(s, &(inv->win));

  /* Paranoia check */
  if (s->windows == TREE_NIL)
    kdprintf(KR_OSTREAM, " Uh-oh!");

  return result;
}

uint32_t
session_Close(Session *s)
{
  /* Go through Session's list of window ids and kill each of the
     corresponding Windows.  Since each call to
     session_WinKill() will actually remove the window id from the
     session's list, just keep iterating while the min of the list is
     not TREE_NIL.  When finished, we can look at session's window
     id list and verify that it's empty. */
  Window *w = NULL;
  TREENODE *tmp = tree_min(s->windows);

  DEBUG(sessclose) {
    kprintf(KR_OSTREAM, "session_Close(): starting iteration w/min ...");
    kprintf(KR_OSTREAM, "     key= %u  value=%u", tmp->value, tmp->data);
  }

  while (tmp != TREE_NIL) {
    TREENODE *z = tree_find(s->windows, tmp->value);
    if (z != TREE_NIL && z->value != 0) {

      DEBUG(sessclose) kprintf(KR_OSTREAM, " calling winid_to_window w/ %u",
			       z->value);
      w = winid_to_window(s, z->value);
      if (w == NULL)
	kdprintf(KR_OSTREAM, "*** Corrupt winid list (winid_to_window) in "
		 "session_Close()");

      DEBUG(sessclose) 
	kprintf(KR_OSTREAM, "session_Close() calling WinKill...");

      session_WinKill(w);
    }
    tmp = tree_min(s->windows);
  }

  /* Clear event queue */
  EvQueue_Clear(&(s->events));

  /* Free session object */
  free(s);

  return RC_OK;
}

uint32_t
session_WinMap(Window *w)
{
  /* If the window is already mapped, this command has no effect.
     Otherwise, map the window. */
  DEBUG(map) kprintf(KR_OSTREAM, "Inside session_WinMap()...");

  window_show(w);

  /* Current "mapping" policy is to give the just-mapped window the
     focus */
  DEBUG(map)
    kprintf(KR_OSTREAM, "session_WinShow() calling winsys_change_focus().\n");

  assert(focus);
  winsys_change_focus(focus, w);

  return RC_OK;
}

uint32_t
session_WinUnmap(Window *w)
{
  window_hide(w);
  return RC_OK;
}

uint32_t
session_WinKill(Window *w)
{
  uint32_t buffer_id = w->buffer_id;
  TREENODE *t;

  assert(w->type == WINTYPE_CLIENT);

  /* First, remove window from session's list of active windows */
  t = tree_find(((Session *)(w->session))->windows, w->window_id);
  if (t == TREE_NIL)
    kdprintf(KR_OSTREAM, "*** Corrupt winid list in session_WinKill().");

  ((Session *)(w->session))->windows = 
    tree_remove(((Session *)(w->session))->windows, t);

  winid_free(w->window_id);

  /* Unmap shared space */  
  return fbm_UnmapClient(buffer_id);
}

uint32_t
session_WinGetSize(Window *w, uint32_t *width, uint32_t *height)
{
  *width = w->size.x;
  *height = w->size.y;
  return RC_OK;
}

uint32_t
session_WinSetClipRegion(Window *w, rect_t area)
{
  w->userClipRegion = area;
  return RC_OK;
}

uint32_t
session_WinSetTitle(Window *w, uint8_t *title)
{
  window_set_name(w, title);
  return RC_OK;
}

uint32_t 
session_NextEvent(Session *s, Event *ev)
{
  DEBUG(events) kprintf(KR_OSTREAM, "Inside session_NextEvent()...");

  if (EvQueue_IsEmpty(&(s->events))) {
    DEBUG(events) kprintf(KR_OSTREAM, "... no events for this session");
    return RC_Session_Retry;
  }
  else {

    DEBUG(events) kprintf(KR_OSTREAM, "... returning an event!");

    if (!EvQueue_Remove(&(s->events), ev)) {
      kprintf(KR_OSTREAM, "** ERROR: empty queue found.");
      return RC_capros_key_RequestError;
    }
  }

  DEBUG(events) kprintf(KR_OSTREAM, "... verify: retrieved event:\n"
			      "  type=%u  winid=0x%08x  processed=%u "
			      "[%u %u %u %u]",
			      ev->type,
			      ev->window_id,
			      ev->processed,
			      ev->data[0], 
			      ev->data[1], 
			      ev->data[2], 
			      ev->data[3]);
			      
  return RC_OK;
}

uint32_t
session_WinRedraw(Window *w, rect_t bounds)
{
  clip_vector_t *regions = NULL;
  uint32_t u;
  rect_t conv;

  /* User only has knowledge of WINTYPE_CLIENT and WINTYPE_INVISIBLE
     windows */
  if (w->type != WINTYPE_CLIENT && w->type != WINTYPE_INVISIBLE)
    kdprintf(KR_OSTREAM, "Predicate failure in session_WinRedraw().\n");

  /* If window is invisible, there's nothing to be drawn (!) */
  if (w->type == WINTYPE_INVISIBLE)
    return RC_OK;

  /* This window must be mapped */
  if (!w->mapped)
    return RC_OK;

  /* All its ancestors must be mapped as well */
  if (!window_ancestors_mapped(w))
    return RC_OK;

  /* Now reconcile the bounds with the visible parts of the
     window. Note that bounds are in window coords! */
  xform_rect(w, bounds, WIN2ROOT, &conv);
  regions = window_get_subregions(w, (UNOBSTRUCTED | INCLUDE_CHILDREN));

  for (u = 0; u < regions->len; u++) {
    rect_t final;

    /* If this visible part doesn't intersect the user-specified
       bounds, skip it */
    if (!rect_intersect(&(regions->c[u].r), &conv, &final))
      continue;

    /* Call drawing routine */
    window_draw(w, final);
  }
  return RC_OK;
}

uint32_t session_WinResize(Window *w, point_t size_delta)
{
  point_t null_delta = {0,0};

  if (w == NULL)
    return RC_capros_key_RequestError;

  w->move(w, null_delta, size_delta);

  return RC_OK;
}

uint32_t 
session_NewSessionCreator(Session *s, Window *container,
			  cap_t kr_new_creator)
{
  /* Make a new wrapper key with the specified container */
  if (s->trusted)
    capros_Process_makeStartKey(KR_SELF, TRUSTED_SESSION_CREATOR_INTERFACE,
			   KR_SESSION_CREATOR);
  else 
    capros_Process_makeStartKey(KR_SELF, SESSION_CREATOR_INTERFACE,
			   KR_SESSION_CREATOR);
  return forwarder_create(KR_CLIENT_BANK, kr_new_creator, KR_NEW_NODE,
			KR_SESSION_CREATOR,
			capros_Forwarder_sendWord,
			ADDRESS(container) );

}

