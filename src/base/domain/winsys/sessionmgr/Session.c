#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>

#include <domain/SessionKey.h>
#include <domain/domdbg.h>

#include <stdlib.h>

#include "../coordxforms.h"
#include "../debug.h"
#include "Session.h"
#include "window_ids.h"

#include "../winsyskeys.h"
#include "../global.h"

static void
session_maybe_wakeup(Session *session)
{
  if (session->waiting) {
    // FIXME: This is broken; retry is no longer implemented.
    node_wake_some_no_retry(KR_PARK_NODE, ADDRESS(session), 0, 
			    ADDRESS(session));
    session->waiting = false;
  }
}

Session *
session_create(Window *container)
{
  Session *new_session = (Session *)malloc(sizeof(Session));

  if (new_session) {
    new_session->windows = TREE_NIL;

    /* No pending event */
    EvQueue_Clear(&(new_session->events));
    new_session->waiting = false;
  }

  /* Set container */
  new_session->container = container;

  new_session->cut_seq = 0;
  new_session->paste_seq = 0;

  return new_session;
}

void
session_queue_mouse_event(Session *session, uint32_t window, 
			  uint32_t button_mask,
			  point_t cursor)
{
  Event event;
  point_t win_point;
  Window *w = winid_to_window(session, window);

  /* Transform cursor coords into Window-relative coords */
  if (w == NULL)
    return;

  xform_point(w, cursor, ROOT2WIN, &win_point);

  DEBUG(mouse) kprintf(KR_OSTREAM, "session_queue_mouse_event(): "
		       "sending client a mouse event at (%d,%d)\n",
		       win_point.x, win_point.y);

  event.type = Mouse;
  event.window_id = window;
  event.data[0] = button_mask;
  event.data[1] = win_point.x;
  event.data[2] = win_point.y;
  event.data[3] = 0;
  event.data[4] = 0;
  event.processed = false;

  EvQueue_Insert(&(session->events), event);

  session_maybe_wakeup(session);
}

void
session_queue_key_event(Session *session, uint32_t window, uint32_t scancode)
{
  Event event;

  DEBUG(keyb)
    kprintf(KR_OSTREAM, "session_queue_key_event(): scancode = 0x%08x\n",
	    scancode);


  event.type = Key;
  event.window_id = window;
  event.data[0] = scancode;
  event.data[1] = 0;
  event.data[2] = 0;
  event.data[3] = 0;
  event.data[4] = 0;
  event.processed = false;

  EvQueue_Insert(&(session->events), event);

  session_maybe_wakeup(session);
}

void
session_queue_resize_event(Session *session, uint32_t window, 
			   point_t new_origin, point_t new_size)
{
  Event event;

  event.type = Resize;
  event.window_id = window;
  event.data[0] = new_origin.x;
  event.data[1] = new_origin.y;
  event.data[2] = new_size.x;
  event.data[3] = new_size.y;
  event.data[4] = 0;
  event.processed = false;

  EvQueue_Insert(&(session->events), event);

  session_maybe_wakeup(session);
}
