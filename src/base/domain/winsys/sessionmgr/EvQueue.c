#include <stddef.h>
#include <eros/target.h>
#include <domain/SessionKey.h>
#include "EvQueue.h"

/* define the following if you don't care if a client keeps up with
   the queue */
#define BASH_OUTPTR

static Event NULL_EVENT = 
  {
    Null,
    0x0u,
    true,
    {0, 0, 0, 0, 0}
  };

void
EvQueue_Clear(EvQueue *evq)
{
  uint32_t u;

  if (evq == NULL)
    return;

  for (u = 0; u < MAXEVENTS; u++)
    evq->list[u] = NULL_EVENT;

  evq->head  = 0;
  evq->tail = -1;
}

static void
incr(uint32_t *index)
{
  (*index)++;
  if (*index == MAXEVENTS)
    *index = 0;
}

bool
EvQueue_Insert(EvQueue *evq, Event ev)
{
  if (evq == NULL)
    return false;

  if (ev.processed == true)
    return false;

  incr(&(evq->tail));

  if ((evq->head == evq->tail) && (evq->list[evq->head].type != Null))
    return false;		/* overflow */
  else
    evq->list[evq->tail] = ev;

  return true;

}

bool
EvQueue_Remove(EvQueue *evq, Event *ev)
{
  if (evq == NULL)
    return false;

  if (ev == NULL)
    return false;

  if (EvQueue_IsEmpty(evq))
    return false;		/* empty queue */

  *ev = evq->list[evq->head];
  evq->list[evq->head] = NULL_EVENT;
  incr(&(evq->head));

  return true;
}

bool
EvQueue_IsEmpty(EvQueue *evq)
{
  if (evq == NULL)
    return true;

  return (evq->list[evq->head].type == Null);
}
