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

/* This package provides the logic for mapping a Window object to a
   unique, opaque window id that can be given to a window system
   client.  The window system client uses its SessionKey plus a window
   id to request window operations (such as drawing, clipping, etc.).
*/
#include <stddef.h>
#include <eros/target.h>

#include <domain/domdbg.h>

#include "window_ids.h"
#include "../debug.h"
#include "../winsyskeys.h"
#include "../global.h"

/* list of all possible window ids and whether each is currently used */
#define MAX_IDS 100
static bool window_ids[MAX_IDS] = { false };

/* Map a window id to its corresponding Window object.  Each Session
   maintains its own list of currently valid window ids.  */
Window * 
winid_to_window(Session *s, uint32_t winid)
{
  Window *found = NULL;
  TREENODE *node = tree_find(s->windows, winid);

  if (node == TREE_NIL) {
    DEBUG(winid) kprintf(KR_OSTREAM, "winid_to_window():** NO WINDOW FOUND!\n"
			 "   for window id %u and sess 0x%08x\n", winid,
			 ADDRESS(s));
    return NULL;
  }

  found = (Window *)(node->data);

  if (found == NULL)
    kdprintf(KR_OSTREAM, "**ERROR: winid_to_window: corrupt winid tree.\n");

  /* Now match the Session id; This shouldn't fail, but we might have
     a corrupted list of Window ids */
  DEBUG(winid)
    kprintf(KR_OSTREAM, 
	    "In winid_to_window(), w[0x%08x] checking s 0x%08x == w->s 0x%08x",
	    ADDRESS(found),
	    ADDRESS(s), ADDRESS(found->session));

  if ( ADDRESS(found->session) == ADDRESS(s))
    return found;
  else {
    DEBUG(winid) kprintf(KR_OSTREAM, 
			 "winid_to_window(): *** SESSION DOESN'T MATCH!");
    return NULL;
  }

  return NULL;
}

/* Find the first unused window id and bind the Window object to that id. */
uint32_t winid_new(Session *s, Window *w)
{
  /* Go through list of current ids.  Find first one available. NOTE:
     id == 0 is not a valid id. */
  uint32_t u;

  for (u = 1; u < MAX_IDS; u++) {
    if (!window_ids[u]) {

      TREENODE *new = id_tree_create();

      new->value = u;
      new->data = ADDRESS(w);

      window_ids[u] = true;
      s->windows = tree_insert(s->windows, new);

      if (s->windows == TREE_NIL)
	kdprintf(KR_OSTREAM, "**ERROR: winid_new(): corrupt winid list.\n");

      DEBUG(winid) kprintf(KR_OSTREAM, "winid_new(): just mapped (%u,0x%08x)\n"
			   "     for sess 0x%08x\n", u, 
			   ADDRESS(w), ADDRESS(s));
      return u;
    }
  } 

  return 0;
}

void
winid_free(uint32_t id)
{
  if (id < MAX_IDS)
    window_ids[id] = false;

}
