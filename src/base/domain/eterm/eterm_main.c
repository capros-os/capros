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

/* A domain that creates a window system window for use as an
   graphical terminal */
#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/cap-instr.h>

#include <string.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Include the needed interfaces */
#include <domain/SessionCreatorKey.h>
#include <domain/SessionKey.h>
#include "idl/capros/eterm.h"
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Stream.h>
#include <idl/capros/Node.h>

#include <graphics/color.h>
#include <graphics/fonts/Font.h>

#include "constituents.h"

#include "eterm_ansi.h"

#undef TRACE

#define KR_OSTREAM          KR_APP(0) /* For debugging output via kprintf*/

#define KR_SESSION          KR_APP(2)
#define KR_SCRATCH          KR_APP(3)
#define KR_SUB_BANK         KR_APP(4)
#define KR_EOUT             KR_APP(5)
#define KR_START            KR_APP(6)
#define KR_SESSION_CREATOR  KR_APP(7)
#define KR_STASH            KR_APP(8)

uint32_t main_window_id = 0;

static uint32_t font_index = 0;

/* Use a keycode queue so we can do things like translate keycodes to
   ANSI escape sequences. */
#define MAXKEYS  10
typedef struct KeyQueue KeyQueue;
struct KeyQueue {
  uint8_t list[MAXKEYS];
  int32_t head;
  int32_t tail;
};

static void queue_clear(KeyQueue *q);
static bool queue_insert(KeyQueue *q, uint8_t c);
static bool queue_remove(KeyQueue *q, uint8_t *c);
static bool queue_empty(KeyQueue *q);

static KeyQueue Q;

static void
return_start_key(cap_t kr_start)
{
  Message msg;

  memset(&msg, 0, sizeof(msg));
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = kr_start;

  SEND(&msg);
}

static bool
ProcessRequest(Message *m)
{
  switch(m->rcv_code) {
  case OC_capros_Stream_read:
    {
      Event evt;
      result_t result;

      /* Check for pending chars in the Q first */
      if (!queue_empty(&Q)) {
	uint8_t c;

	queue_remove(&Q, &c);
	m->snd_w1 = c;
	m->snd_code = RC_OK;
	return true;
      }

      /* Stay in this loop until we actually have data to pass on to
	 the stream reader */
      for (;;) {

	/* Events are dispatched on Sessions... */
	result = session_next_event(KR_SESSION, &evt);
	if (result != RC_OK)
	  kprintf(KR_OSTREAM, "** ERROR: session_next_event() "
		  "result=%u", result);
	else {
#ifdef TRACE
	  kprintf(KR_OSTREAM, "Received %s event!",
		  (evt.type == Resize ? "RESIZE" :
		   (evt.type == Mouse ? "MOUSE" : "KEY")));
#endif

	  /* Since this domain only has one window, if the received
	     event somehow doesn't pertain to this one window, ignore it. */
	  if (evt.window_id != main_window_id)
	    continue;

	  /* Mouse events */

	  if (IS_MOUSE_EVENT(evt)) {
	    /* Ignore for now */
	    continue;
	  }

	  /* Keyboard events */

	  else if (IS_KEY_EVENT(evt)) {
	    uint32_t keycode = evt.data[0];

#ifdef TRACE 
	    kprintf(KR_OSTREAM, "Key event: %d\n", (int32_t)evt.data[0]);
#endif

	    /* Intercept special keys (like FN1) */
	    if (keycode > 255) {
	      switch(keycode-255) {
	      case 0x1b:	/* F1 */
		{
		  COPY_KEYREG(KR_RETURN, KR_STASH);
		  font_index++;
		  font_index = font_index % font_num_fonts();
		  eros_domain_eterm_set_font(KR_EOUT, font_index);
		  COPY_KEYREG(KR_STASH, KR_RETURN);
		}
		break;

	      case 0x1c:	/* F2 */
		{
		}
		break;

		/* Arrow keys get turned into ANSI escape sequences */
	      case 0x4f:	/* Left Arrow = "ESC[1D" */
		{
		  keycode = ESC;
		  queue_insert(&Q, '[');
		  queue_insert(&Q, '1');
		  queue_insert(&Q, 'D');
		}
		break;

	      case 0x51:	/* Right Arrow = "ESC[1C" */
		{
		  keycode = ESC;
		  queue_insert(&Q, '[');
		  queue_insert(&Q, '1');
		  queue_insert(&Q, 'C');
		}
		break;

	      case 0x4c:	/* Up Arrow = "ESC[1A" */
		{
		}
		break;

	      case 0x54:	/* Down Arrow = "ESC[1B" */
		{
		}
		break;

	      default:
		break;
	      }
	    }

	    m->snd_w1 = keycode;
	    m->snd_code = RC_OK;
	    return true;
	  }

	  /* Resize events */
	  else if (IS_RESIZE_EVENT(evt)) {

	    COPY_KEYREG(KR_RETURN, KR_STASH);
	    eros_domain_eterm_resize(KR_EOUT,
				     EVENT_NEW_WIDTH(evt),
				     EVENT_NEW_HEIGHT(evt));
	    COPY_KEYREG(KR_STASH, KR_RETURN);
	  }
	}
      }
    }
    break;

  default:
    {
      m->snd_code = RC_capros_key_UnknownRequest;
    }
  }

  return true;
}

static void
process_loop()
{
  Message msg;

  memset(&msg, 0, sizeof(Message));
  msg.rcv_rsmkey = KR_RETURN;
  msg.snd_invKey = KR_RETURN;

  do {
    RETURN(&msg);
  } while (ProcessRequest(&msg));
}

int
main(void)
{
  /* A key to a session is passed via this domain's constructor.  Grab
     it here and stash it for later use. */
  COPY_KEYREG(KR_ARG(0), KR_SESSION_CREATOR);

  /* Stash the resume key */
  COPY_KEYREG(KR_RETURN, KR_STASH);

  /* Retrieve the constituent keys */
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_EOUT, KR_EOUT);

  /* Create a subbank to use for window creation */
  if (capros_SpaceBank_createSubBank(KR_BANK, KR_SUB_BANK) != RC_OK) {
    kprintf(KR_OSTREAM, "Eterm_Main failed to create sub bank.\n");
    return -1;
  }

  kprintf(KR_OSTREAM, "ETerm now constructing eout...\n");

  /* Make start key to pass to eterm_out */
  process_make_start_key(KR_SELF, 0, KR_START);
  if (constructor_request(KR_EOUT, KR_BANK, KR_SCHED, KR_START, 
			  KR_EOUT) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: Eterm_Main couldn't construct eout!\n");
    return -1;
  }

  /* Create new session */
  if (session_creator_new_session(KR_SESSION_CREATOR, KR_BANK, 
				  KR_SESSION) != RC_OK) {
    kprintf(KR_OSTREAM, "Eterm failed to create new session.\n");
    return -1;
  }

  kprintf(KR_OSTREAM, "Eterm now initializing eout...\n");

  /* Initialize as needed */
  if (eros_domain_eterm_initialize(KR_EOUT,
				   KR_SESSION,
				   &main_window_id) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: Eterm_Main couldn't initialize Eterm_Out\n");
    return -1;
  }

  /* Make sure we return ETERM start key to constructor! */
  COPY_KEYREG(KR_STASH, KR_RETURN);
  return_start_key(KR_EOUT);

  /* Initialize the keycode Q */
  queue_clear(&Q);

  kprintf(KR_OSTREAM, "Eterm_Main is ready...\n");

  /* Now this domain becomes the "event gettor".  As such, we enter
     a processing loop, waiting for the request to get an event. */
  process_loop();

  return 0;
}

void
queue_clear(KeyQueue *evq)
{
  uint32_t u;

  if (evq == NULL)
    return;

  for (u = 0; u < MAXKEYS; u++)
    evq->list[u] = 0;

  evq->head  = 0;
  evq->tail = -1;
}

static void
incr(uint32_t *index)
{
  (*index)++;
  if (*index == MAXKEYS)
    *index = 0;
}

bool
queue_insert(KeyQueue *evq, uint8_t c)
{
  bool first = false;

  if (evq == NULL)
    return false;

  first = evq->tail < 0;

  incr(&(evq->tail));

  if ((evq->head == evq->tail) && (evq->list[evq->head] != 0))
    return false;		/* overflow */
  else
    evq->list[evq->tail] = c;

  return true;

}

bool
queue_remove(KeyQueue *evq, uint8_t *c)
{
  if (evq == NULL)
    return false;

  if (c == NULL)
    return false;

  if (queue_empty(evq))
    return false;		/* empty queue */

  *c = evq->list[evq->head];
  evq->list[evq->head] = 0;

  incr(&(evq->head));

  return true;
}

bool
queue_empty(KeyQueue *evq)
{
  if (evq == NULL)
    return true;

  return (evq->list[evq->head] == 0);
}

#if 0
void
queue_dump(KeyQueue *evq)
{
  uint32_t u;

  if (evq == NULL)
    return;

  kprintf(KR_OSTREAM, "** Dump of KeyEvent Queue: (head = %d tail = %d)\n",
	  evq->head, evq->tail);
  for (u = 0; u < MAXKEYS; u++)
    kprintf(KR_OSTREAM, "     list[%u]= 0x%08x\n", u, evq->list[u]);
}
#endif
