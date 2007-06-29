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
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/cap-instr.h>

#include <idl/eros/key.h>

#include <domain/SessionKey.h>
#include <domain/domdbg.h>
#include <domain/SpaceBankKey.h>

#include <stdlib.h>

#include "SessionRequest.h"
#include "window_ids.h"
#include "../debug.h"
#include "../winsyskeys.h"
#include "../global.h"
#include "../PasteBuffer.h"
#include "../coordxforms.h"
#include "../Window.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

/* Receive buffer for invocations */
extern uint32_t receive_buffer[];

/* Declared in winsys.c */
extern PasteBufferState pastebuffer_state;
extern Window *Root;
extern Event send_event;
extern bool winsys_map_addrspace(uint32_t winid, uint32_t kr_slot);
extern bool winsys_demap_addrspace(uint32_t winid);
extern bool winsys_dump_contents(uint32_t winid);

static point_t default_origin = {25, 25};
static point_t default_offset = {20, 20};
static point_t default_size   = {200, 100};

/* FIX: This needs to be implemented (Eric?) */
static bool
is_confined(cap_t kr_cap)
{
  uint32_t keyType;
  result_t result = eros_key_getType(kr_cap, &keyType);

  if (result != RC_OK || keyType == RC_eros_key_Void)
    return false;

  /* FIX: need to insert the action confinement test */
  return true;
}

static bool
ExistingWindowRequest(Session *sess, Message *msg)
{
  /* Use these two variables to ensure that the message wasn't
     truncated! */
  uint32_t expect;
  uint32_t got;

  uint32_t winid = msg->rcv_w1;
  Window *win = winid_to_window(sess, winid);

  if (win == NULL) {
    msg->snd_code = RC_eros_key_RequestError;
    return true;
  }

  switch (msg->rcv_code) {

    /* Temporary */
  case 1999:
    {
      kprintf(KR_OSTREAM, "SessionRequest: unmapping subspace...\n");
      winsys_demap_addrspace(winid);
      msg->snd_code = RC_OK;
    }
    break;

  case OC_Session_WinMap:
    {
#if 0
      if (winsys_dump_contents(win->window_id) == false)
	kprintf(KR_OSTREAM, "*** SessionRequest:  winsys proc keeper invoked"
		" successfully!\n");
#endif
      /* Decoration window will get drawn as a side-effect of whether
	 the client receives focus or not. */
      msg->snd_code = session_WinMap(win);
      msg->snd_code = RC_OK;
    }
    break;

  case OC_Session_WinUnmap:
    {
      /* If there's a decoration, just hide it and all its children
	 will be hidden as well. */
      if (win->parent->type == WINTYPE_DECORATION)
	msg->snd_code = session_WinUnmap(win->parent);
      else 
	msg->snd_code = session_WinUnmap(win);
    }
    break;

  case OC_Session_WinKill:
    {
      if (win->parent->type == WINTYPE_DECORATION)
	msg->snd_code = window_destroy(win->parent, true);
      else 
	msg->snd_code = window_destroy(win, true);
    }
    break;

  case OC_Session_WinGetSize:
    {
      uint32_t width;
      uint32_t height;

      msg->snd_code = session_WinGetSize(win, &width, &height);
      if (msg->snd_code == RC_OK) {
	msg->snd_w1 = width;
	msg->snd_w2 = height;
      }
    }
    break;

  case OC_Session_WinSetClipRegion:
    {
      rect_t area = { {receive_buffer[0], receive_buffer[1]}, 
		      {receive_buffer[2], receive_buffer[3]} };

      expect = 4 * sizeof(uint32_t);
      got = min(msg->rcv_sent, msg->rcv_limit);
      if (got != expect) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      msg->snd_code = session_WinSetClipRegion(win, area);
    
    }
    break;

  case OC_Session_WinSetTitle:
    {
      /* Set name of this Window */
      session_WinSetTitle(win, (uint8_t *)receive_buffer);

      /* Set name of parent only if it's a decoration */
      if (win->parent && win->parent->type == WINTYPE_DECORATION) {
	Window *dec = win->parent;
	rect_t decwin;

	msg->snd_code = session_WinSetTitle(dec,(uint8_t *)receive_buffer);
	/* Redraw the decoration, so the new name is visible as soon
	   as possible! */
	if (dec->mapped && window_ancestors_mapped(dec)) {
	  xform_win2rect(dec, WIN2ROOT, &decwin);
	  win->parent->draw(win->parent, decwin);
	}
      }
    }
    break;

  case OC_Session_WinRedraw:
    {
      rect_t bounds = { {receive_buffer[0], receive_buffer[1]},
                        {receive_buffer[2], receive_buffer[3]} };

      expect = 4 * sizeof(uint32_t);
      got = min(msg->rcv_sent, msg->rcv_limit);
      if (got != expect) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      msg->snd_code = session_WinRedraw(win, bounds);
    }
    break;

  case OC_Session_WinResize:
    {
      point_t new_size = {receive_buffer[0], receive_buffer[1]}; 
      point_t delta;

      expect = 2 * sizeof(uint32_t);
      got = min(msg->rcv_sent, msg->rcv_limit);
      if (got != expect) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      /* Need to compute delta, because that's what the underlying
	 code relies on */
      delta.x = new_size.x - win->size.x;
      delta.y = new_size.y - win->size.y;

      /* Resize decoration if present */
      if (win->parent->type == WINTYPE_DECORATION)
	msg->snd_code = session_WinResize(win->parent, delta);
      else
	msg->snd_code = session_WinResize(win, delta);
    }
    break;

  case OC_Session_WinDragAndDrop:
    {
      /* Not implemented yet: Winsys needs to deliver all mouse events
         up to and including very next mouse-up to originating client
         window AND first window currently underneath cursor. */
      msg->snd_code = RC_eros_key_RequestError;
    }
    break;

  default:
    {
      msg->snd_code = RC_eros_key_RequestError;
    }
    break;
  }
  return true;
}

bool
SessionRequest(Message *msg)
{
  /* Use these two variables to ensure that the message wasn't
     truncated! */
  uint32_t expect;
  uint32_t got;
  Session *sess = (Session *)msg->rcv_w3;

  switch (msg->rcv_code) {
  case OC_Session_NewDefaultWindow:
    {
      uint32_t winid = 0;
      point_t orig = default_origin;
      point_t size = default_size;
      uint32_t decs = (WINDEC_TITLEBAR | WINDEC_BORDER | WINDEC_RESIZE);
      Window *parent = NULL;

      if (msg->rcv_w1 == DEFAULT_PARENT)
	parent = sess->container;
      else {
	parent = winid_to_window(sess, msg->rcv_w1);

	if (parent == NULL) {
	  msg->snd_code = RC_eros_key_RequestError;
	  return true;
	}
      }

      /* Make sure subsequent default windows don't hide each other */
      default_origin.x += default_offset.x;
      default_origin.y += default_offset.y;

      msg->snd_code = session_NewWindow(sess, KR_CLIENT_BANK, 
					parent, orig, size,
					decs, &winid,
					KR_NEW_WINDOW);

      if (msg->snd_code == RC_OK) {
	msg->snd_w1 = winid;
	/* The (wrapper) key to the address space corresponding to the
	   new window's contents */
	msg->snd_key0 = KR_NEW_WINDOW;
      }
    }
    break;

  case OC_Session_NewWindow:
    {
      uint32_t winid = 0;
      point_t orig = {receive_buffer[0], receive_buffer[1]};
      point_t size = {receive_buffer[2], receive_buffer[3]};
      uint32_t decs = receive_buffer[4];
      Window *parent = NULL;

      if (msg->rcv_w1 == DEFAULT_PARENT)
	parent = sess->container;
      else {
	parent = winid_to_window(sess, msg->rcv_w1);

	if (parent == NULL) {
	  msg->snd_code = RC_eros_key_RequestError;
	  return true;
	}
      }

      expect = 5 * sizeof(uint32_t);
      got = min(msg->rcv_sent, msg->rcv_limit);
      if (got != expect) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      if (size.x < MIN_WINDOW_WIDTH || size.y < MIN_WINDOW_HEIGHT) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      DEBUG(session_cmds) 
        kprintf(KR_OSTREAM, "Calling session_NewWindow().\n");
      msg->snd_code = session_NewWindow(sess, KR_CLIENT_BANK, parent,
					orig, size,
					decs, &winid,
					KR_NEW_WINDOW);

      if (msg->snd_code == RC_OK) {
	msg->snd_w1 = winid;
	/* The (wrapper) key to the address space corresponding to the
	   new window's contents */
	msg->snd_key0 = KR_NEW_WINDOW;
      }
    }
    break;

  case OC_Session_NewInvisibleWindow:
    {
      uint32_t winid = 0;
      point_t orig = {receive_buffer[0], receive_buffer[1]};
      point_t size = {receive_buffer[2], receive_buffer[3]};
      uint32_t qualifier = receive_buffer[4];

      Window *parent = NULL;

      if (msg->rcv_w1 == DEFAULT_PARENT)
	parent = sess->container;
      else {
	parent = winid_to_window(sess, msg->rcv_w1);

	if (parent == NULL) {
	  msg->snd_code = RC_eros_key_RequestError;
	  return true;
	}
      }

      expect = 5 * sizeof(uint32_t);
      got = min(msg->rcv_sent, msg->rcv_limit);
      if (got != expect) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      if (size.x < MIN_WINDOW_WIDTH || size.y < MIN_WINDOW_HEIGHT) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      DEBUG(session_cmds) 
        kprintf(KR_OSTREAM, "Calling session_NewInvisibleWindow().\n");
      msg->snd_code = session_NewInvisibleWindow(sess, parent, orig, size, 
						 qualifier, &winid);

      if (msg->snd_code == RC_OK)
	msg->snd_w1 = winid;

    }
    break;

  case OC_Session_Close:
    {
      msg->snd_code = session_Close(sess);

      /* Now, attempt to return allocated storage to client's space
	 bank.  This may fail, but at least we tried. */
      kprintf(KR_OSTREAM, "winsys(): Terminating session...");
      eros_Forwarder_getSlot(KR_ARG(2), STASH_CLIENT_BANK, KR_SCRATCH);

      /* Bash the wrapped key with a void key */
      eros_Forwarder_swapTarget(KR_ARG(2), KR_VOID, KR_VOID);

#if 0
      /* FIX: This code is successful, but any further invocation
      attempts by the client on its session key results in an
      assertion failure: client's keybits are not prepared. */
      if (eros_SpaceBank_free1(KR_SCRATCH, KR_ARG(2)) != RC_OK) {
	kprintf(KR_OSTREAM, "    ... Couldn't return node, so bashing it.");
      }
      else
	kprintf(KR_OSTREAM, "    ... Node returned successfully to client.");
#endif

      free(sess);
    }
    break;

  case OC_Session_NextEvent:
    {
      msg->snd_code = session_NextEvent(sess, &send_event);
      if (msg->snd_code == RC_Session_Retry) {

	msg->invType  = IT_Retry;
	msg->snd_w1   = RETRY_SET_LIK | RETRY_SET_WAKEINFO;

	/* Use the Session address as a unique wake info value */
	msg->snd_w2   = ADDRESS(sess);

	msg->snd_key0 = KR_PARK_WRAP;

	/* Mark a bit in the Session object to indicate waiting */
	sess->waiting = true;
      }
      else if (msg->snd_code == RC_OK) {

	DEBUG(events) kprintf(KR_OSTREAM, "Inside SessionRequest: "
			      " verifying event:\n "
			      "   type=%u winid=0x%08x "
			      " processed=%u",
			      send_event.type,
			      send_event.window_id, 
			      send_event.processed);

	msg->snd_data = &send_event;
	msg->snd_len = sizeof(Event);
	msg->invType = IT_PReturn;
	msg->snd_w1 = 0;
	msg->snd_key0 = KR_VOID;
      }
    }
    break;

  case OC_Session_DisplaySize:
    {
      msg->snd_w1 = sess->container->size.x;
      msg->snd_w2 = sess->container->size.y;
      msg->snd_code = RC_OK;
    }
    break;

  case OC_Session_NewSessionCreator:
    {
      Window *win = winid_to_window(sess, msg->rcv_w1);

      /* Ensure window id is valid for this Session */
      if (win == NULL) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      DEBUG(session_cmds) 
        kprintf(KR_OSTREAM, "Calling session_NewSessionCreator().\n");

      msg->snd_code = session_NewSessionCreator(sess, win, KR_SCRATCH);

      if (msg->snd_code == RC_OK) 
	msg->snd_key0 = KR_SCRATCH;
      else
	kprintf(KR_OSTREAM, "***Uh-oh: result = 0x%08x (%u)\n",
		msg->snd_code, msg->snd_code);
    }
    break;

  case OC_Session_PutPasteBuffer:
    {
      /* The two capabilities are in KR_ARG(0) and KR_ARG(1).  Must
	 ensure they are confined before proceeding! */
      if (!is_confined(KR_ARG(0))) {
	pastebuffer_set_confined(&pastebuffer_state, false);
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      if (!is_confined(KR_ARG(1))) {
	pastebuffer_set_confined(&pastebuffer_state, false);
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      kprintf(KR_OSTREAM, "COPY: Both caps are confined.\n");
      kprintf(KR_OSTREAM, "COPY: Checking sess [%u] w/pastebuffer [%u]\n",
	      sess->cut_seq, pastebuffer_state.cur_seq);

      /* Now check authority of this Session */
      if (sess->cut_seq >= pastebuffer_state.cur_seq) {

	pastebuffer_state.cur_seq = sess->cut_seq;

	kprintf(KR_OSTREAM, "COPY: Just stashed the caps w/seq [%d].\n",
		pastebuffer_state.cur_seq);

	/* Stash the capabilities */
	COPY_KEYREG(KR_ARG(0), KR_PASTE_CONTENT);
	COPY_KEYREG(KR_ARG(1), KR_PASTE_CONVERTER);
	pastebuffer_set_confined(&pastebuffer_state, true);

	msg->snd_code = RC_OK;
      }
      else 
	msg->snd_code = RC_eros_key_RequestError;
    }
    break;

  case OC_Session_GetPasteBuffer:
    {
      /* Ensure stashed capabilities are kosher. FIX: Is checking the
	 state.caps_confined bit sufficient?? */
      if (!is_confined(KR_PASTE_CONTENT)) {
	pastebuffer_set_confined(&pastebuffer_state, false);
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      if (!is_confined(KR_PASTE_CONVERTER)) {
	pastebuffer_set_confined(&pastebuffer_state, false);
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      kprintf(KR_OSTREAM, "PASTE: caps are still confined.\n");
      kprintf(KR_OSTREAM, "PASTE: checking sess [%d] w/pastebuffer [%d]\n",
	      sess->paste_seq, pastebuffer_state.cur_seq);

      if (sess->paste_seq  == pastebuffer_state.cur_seq) {

	kprintf(KR_OSTREAM, "PASTE: returning caps.\n");

	/* Return the capabilities */
	msg->snd_key0 = KR_PASTE_CONTENT;
	msg->snd_key1 = KR_PASTE_CONVERTER;
	msg->snd_code = RC_OK;
      }
      else 
	msg->snd_code = RC_eros_key_RequestError;

    }
    break;

  default:
    {
      return ExistingWindowRequest(sess, msg);
    }

  }
  return true;
}

