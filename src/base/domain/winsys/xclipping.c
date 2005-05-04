#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <domain/domdbg.h>
#include <string.h>
#include <stdlib.h>

#include "xclipping.h"

#include "coordxforms.h"
#include "sessionmgr/Session.h"

#include "global.h"
#include "debug.h"
#include "winsyskeys.h"

/* declared and managed in winsys.c */
extern Window *Root;

void
vector_dump(clip_vector_t *list)
{
  uint32_t i;

  kprintf(KR_OSTREAM, "vector_dump(): num in vector: %u\n", list->len);

  for (i = 0; i < list->len; i++) {
    kprintf(KR_OSTREAM, "[%u]=> [(%d,%d) (%d,%d)] %s\n",
	    i, list->c[i].r.topLeft.x, list->c[i].r.topLeft.y,
	    list->c[i].r.bottomRight.x, list->c[i].r.bottomRight.y,
	    list->c[i].in ? "IN" : "OUT");
  }
}

clip_vector_t *
window_compute_clipping(Window *w, clip_vector_t **unobstructed, uint32_t flag)
{
  clip_vector_t *obstructed = make_clip_vector(0);
  Link *siblink = NULL;
  Link *end_of_chain = NULL;
  Window *tmp = w;
  Window *sibwin = NULL;
  rect_t rect;
  uint32_t i;
  bool finished = WINDOWS_EQUAL(tmp, Root);

  /* Check entire window hierarchy as needed */
  while (!finished) {

    /* Check immediate siblings */
    if (flag == OBSTRUCTING_OTHERS)
      siblink = tmp->sibchain.next;
    else
      siblink = tmp->sibchain.prev;

    end_of_chain = (&(tmp->parent->children));

    while (ADDRESS(siblink) != ADDRESS(end_of_chain)) {

      sibwin = (Window *)siblink;
      if (sibwin->mapped) {

	/* Convert Window to rectangle in Root window coords */
	if (!xform_win2rect(sibwin, WIN2ROOT, &rect))
	  kdprintf(KR_OSTREAM, "**FATAL: couldn't xform window\n");

	DEBUG(clip) kprintf(KR_OSTREAM, "window_compute_clipping():\n"
			    "      clipping [(%d,%d) (%d,%d)] against the list",
			    rect.topLeft.x, rect.topLeft.y, 
			    rect.bottomRight.x, rect.bottomRight.y);

	*unobstructed = clip(&rect, *unobstructed);

	DEBUG(clip) {
	  kprintf(KR_OSTREAM, "         Before extracting:\n");
	  vector_dump(*unobstructed);
	}

	if (obstructed->len == 0)
	  obstructed = vector_remove_rects(unobstructed, true);
	else
	  obstructed = vector_concat(&obstructed, 
				     vector_remove_rects(unobstructed,
							 true));

	if (flag == OBSTRUCTING_OTHERS) {
	  for (i = 0; i < obstructed->len; i++)
	    window_draw(sibwin, obstructed->c[i].r);

	  /* Now we've processed the 'obstructed' list and need to
	     zero it out */
	  obstructed = make_clip_vector(0);
	}

	DEBUG(clip) {
	  kprintf(KR_OSTREAM, "         After extracting:\n");
	  vector_dump(*unobstructed);
	  kprintf(KR_OSTREAM, "\n        And obstructed is:\n");
	  vector_dump(obstructed);
	}
      }
      /* Update the pointer and repeat; Note we're going in the
       *prev* direction to find sibchain Windows in front and going
       in the *next* direction to find sibchain Windows
       behind... */
      if (flag == OBSTRUCTING_OTHERS)
	siblink = siblink->next;
      else
	siblink = siblink->prev;
   }

    /* Advance pointer for next iteration */
    tmp = tmp->parent;
    finished = WINDOWS_EQUAL(tmp, Root);
  }

  if (flag == OBSTRUCTED)
    return obstructed;

  if (flag & UNOBSTRUCTED || flag == OBSTRUCTING_OTHERS)
    return *unobstructed;

  /* Should never get to this point */
  kdprintf(KR_OSTREAM, "** FATAL Error in window_compute_clipping().\n");

  return NULL;
}

/* Used to determine which subregions of the specified window are
   either obstructed by other windows or unobstructed by other windows
   (depending on the specified flag).  */
clip_vector_t *
window_get_subregions(Window *w, uint32_t flag)
{
  clip_vector_t *unobstructed = make_clip_vector(0);
  rect_t orig_clip;

  if (w == NULL)
    kdprintf(KR_OSTREAM, "Predicate failure in window_get_subregions():\n\t"
	     " w is NULL\n");

  if (!window_clip_to_ancestors(w, &orig_clip))
    return make_clip_vector(0);

  /* If requested, don't include the rectangular regions that
     represent this window's children. (This should only be done via a
     session_win_redraw call on a client window.) */
  if (flag & INCLUDE_CHILDREN) {
    Link *childlink = w->children.next;
    rect_t rect;
    clip_vector_t *vec = make_clip_vector(0);

    if (w->type != WINTYPE_CLIENT || !(flag & UNOBSTRUCTED))
      kdprintf(KR_OSTREAM, "Predicate failure in window_get_subregions().\n");

    /* Start with entire bounds */
    vector_append_rect(&vec, &orig_clip, true);

    while (ADDRESS(childlink) != ADDRESS(&(w->children))) {
      Window *childwin = (Window *)childlink;

      if (childwin->mapped) {
	/* Convert Window to rectangle in Root window coords */
	if (!xform_win2rect(childwin, WIN2ROOT, &rect))
	  continue;

	/* We need to essentially remove all rectangular
	   sub-sections of the Window that represent its mapped
	   children. To do this, first call the clip routine.  Then,
	   we need to iterate the resulting vector and manually
	   remove the subregion corresponding to the child
	   window. */
	vec = clip(&rect, vec);

	{
	  int32_t z;
	  rect_t ri;

	  rect_intersect(&rect, &orig_clip, &ri);
	  for (z = 0; z < vec->len; z++) {

	    if (rect_ne(&(vec->c[z].r), &ri)) 
	      vector_append_rect(&unobstructed, &(vec->c[z].r), true);
	  }
	}
	vec = unobstructed;
	unobstructed = make_clip_vector(0);
      }
      childlink = childlink->next;
    }
    unobstructed = vec;
  }
  else {
    /* Add clipped rectangle as first entry in clip vector (which
       means assume that the entire Window (clipped only by its
       parent) is visible) */
    vector_append_rect(&unobstructed, &orig_clip, true);
  }

  return window_compute_clipping(w, &unobstructed, flag);
}

/* Generate list of rectangular regions that will be newly exposed
   once the given window is moved/resized according to 'new_boundary' */
clip_vector_t *
window_newly_exposed(Window *w, rect_t new_boundary)
{
  clip_vector_t *unobstructed = window_get_subregions(w, UNOBSTRUCTED);
  clip_vector_t *exposelist = NULL;
  clip_vector_t *tmplist = NULL;


  DEBUG(expose) {
    kprintf(KR_OSTREAM, "window_newly_exposed(): num in unobstructed = %u\n",
	    unobstructed->len);
    vector_dump(unobstructed);

    kprintf(KR_OSTREAM, "... about to clip with rect = [(%d,%d)(%d,%d)]\n",
	    new_boundary.topLeft.x, new_boundary.topLeft.y,
	    new_boundary.bottomRight.x, new_boundary.bottomRight.y);
  }

  /* Manually clip original location of the window and its
     decorations by the new location */
  tmplist = clip(&new_boundary, unobstructed);

  DEBUG(expose) {
    kprintf(KR_OSTREAM, " ... dump of tmplist before removing 'false':\n");
    vector_dump(tmplist);
  }

  /* Remove those subregions of original location that are
     common to both (and return a list of the leftovers) */
  exposelist = vector_remove_rects(&tmplist, false);

  DEBUG(expose) {
    kprintf(KR_OSTREAM, " ... tmplist dump *after* removing 'false':\n");
    vector_dump(tmplist);
    kprintf(KR_OSTREAM, " ... exposelist dump *after* removing 'false':\n");
    vector_dump(exposelist);
  }


  DEBUG(expose) {
    kprintf(KR_OSTREAM, " ...   tmplist after handling decorations:\n");
    vector_dump(tmplist);
    kprintf(KR_OSTREAM, " ...   exposelist after handling decorations:\n");
    vector_dump(exposelist);
  }

  return exposelist;
}

