#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <domain/domdbg.h>
#include <string.h>
#include <stdlib.h>

#include "Window.h"
#include "Decoration.h"

#include "coordxforms.h"
#include "sessionmgr/Session.h"
#include "sessionmgr/SessionRequest.h"
#include "fbmgr/fbm_commands.h"

#include "global.h"
#include "debug.h"
#include "winsyskeys.h"
#include "xclipping.h"

/* declared and managed in winsys.c */
extern Window *Root;
extern Window *focus;

extern void winsys_change_focus(Window *to, Window *from);

static void
window_clear_title(Window *thisWindow)
{
  uint32_t u;

  for (u = 0; u < MAX_TITLE; u++)
    thisWindow->name[u] = '\0';
}

void 
window_move_to_back(Window *thisWindow)
{
  Window *parent = thisWindow->parent;

  /* Find those subregions of thisWindow that represent other windows
     that are directly behind thisWindow.  Expose each such
     subregion. */
  window_get_subregions(thisWindow, OBSTRUCTING_OTHERS);

  link_Unlink(&thisWindow->sibchain);
  link_insertBefore(&parent->children, &thisWindow->sibchain);
}

void 
window_bring_to_front(Window *thisWindow)
{
  Window *parent = thisWindow->parent;

  /* Plan of attack: before bringing window to front, compute its
     "soon-to-be-exposed" areas.  Then, bring window to front and
     redraw each of those areas.
  */
  clip_vector_t *expose_zones = NULL;
  bool drawing = window_ancestors_mapped(thisWindow);

  if (drawing) {
    expose_zones = window_get_subregions(thisWindow, OBSTRUCTED);

    DEBUG(front) {
      kprintf(KR_OSTREAM, "window_bring_to_front(): winid=%u\n",
	      thisWindow->window_id);

      kprintf(KR_OSTREAM, "   num exposed areas = %u\n",
	      expose_zones->len);
    }
  }

  /* Now, update the links, which actually reorders the windows. */
  link_Unlink(&thisWindow->sibchain);
  link_insertAfter(&parent->children, &thisWindow->sibchain);

  /* Redraw each exposed area */
  if (drawing) {
    uint32_t i;
    for (i = 0; i < expose_zones->len; i++) {

      if (expose_zones->c[i].in) {
	DEBUG(front) kprintf(KR_OSTREAM, "winsys: expose zones, redrawing "
			     "for winid=0x%08x\n"
			     "          [(%d,%d) (%d,%d)]", 
			     thisWindow->window_id,
			     expose_zones->c[i].r.topLeft.x,
			     expose_zones->c[i].r.topLeft.y,
			     expose_zones->c[i].r.bottomRight.x,
			     expose_zones->c[i].r.bottomRight.y);

	window_draw(thisWindow, expose_zones->c[i].r);
      }
    }
  }

  DEBUG(front) kprintf(KR_OSTREAM, "window_bring_to_front: new front "
		       "0x%08x %d\n", ADDRESS(thisWindow->session),
		       thisWindow->window_id);

}

/* Here's the policy for handing off focus when a window is hidden.
   First, search window's sibling list for next mapped sibling (depth
   wise).  If none found, go to window's parent and repeat.  Worst
   case, the Root window gets focus. Also: never give an invisible
   window focus! */
static Window *
window_focus_next_in_line(Window *w)
{
  Link *item = w->sibchain.next;
  Window *z = NULL;

  DEBUG(focus)
    kprintf(KR_OSTREAM, "window_focus_next_in_line() w/ 0x%08x type=[%s]\n",
	    ADDRESS(w), w->type == WINTYPE_DECORATION ? "DEC" : 
	    w->type == WINTYPE_BUTTON ? "BUTTON" : "OTHER");

  /* Try 'next' direction first */
  while (ADDRESS(item) != ADDRESS(&(w->parent->children))){
    z =(Window *)item;

    DEBUG(focus)
       kprintf(KR_OSTREAM, "... Trying 'next' siblink: 0x%08x type=[%s]\n", 
	       ADDRESS(z), z->type == WINTYPE_DECORATION ? "DEC" : 
	    z->type == WINTYPE_BUTTON ? "BUTTON" : "OTHER");

    if (z->mapped && z->type != WINTYPE_INVISIBLE)
      return z;

    item = item->next;
  }

  /* Try 'prev' direction next */
  item = w->sibchain.prev;
  while (ADDRESS(item) != ADDRESS(&(w->parent->children))){
    z =(Window *)item;

    DEBUG(focus)
      kprintf(KR_OSTREAM, "... Trying 'prev' child: 0x%08x\n", ADDRESS(z));

    if (z->mapped)
      return z;

    item = item->prev;
  }

  /* None found, try parent */
  if (w->parent->mapped) {

    DEBUG(focus)
      kprintf(KR_OSTREAM, "... Returning parent: 0x%08x\n", ADDRESS(z));

    return w->parent;
  }


  DEBUG(focus)
    kprintf(KR_OSTREAM, "... Recursive call w/parent:\n");

  return window_focus_next_in_line(w->parent);
}

/* For the specified pieces of thisWindow, ask window system to redraw
   any underlying windows and decorations */
void
window_unmap_pieces(Window *thisWindow, Window *parent, clip_vector_t **pieces)
{
  clip_vector_t *parent_subregions = make_clip_vector(0);
  uint32_t i;

  if (thisWindow == NULL || parent == NULL)
    return;

  DEBUG(unmap) {
    kprintf(KR_OSTREAM, "window_unmap_pieces(): dump of pieces:\n");
    vector_dump(*pieces);
  }

  /* Clip the vector against windows which are obstructed by
     these regions. Whatever's left is the list of subregions of
     thisWindow's parent that need to be exposed. */
  parent_subregions = window_compute_clipping(thisWindow, pieces, 
					      OBSTRUCTING_OTHERS);

  DEBUG(unmap) {
    kprintf(KR_OSTREAM, "window_unmap_pieces() parent_subregions =>");
    vector_dump(parent_subregions);
  }

  /* Redraw parent pieces */
  for (i = 0; i < parent_subregions->len; i++)
    parent->draw(parent, parent_subregions->c[i].r);
}

void 
window_hide(Window *thisWindow)
{
  clip_vector_t *unmap_main = make_clip_vector(0);

  if (thisWindow == NULL || thisWindow->parent == NULL)
    return;

  if (!thisWindow->mapped)
    return;

  /* First find the currently unobstructed regions  */
  unmap_main = window_get_subregions(thisWindow, UNOBSTRUCTED);

  /* Redraw what's behind those regions */
  window_unmap_pieces(thisWindow, thisWindow->parent, &unmap_main);

  /* Set mapped flag */
  thisWindow->mapped = false;

  /* Transfer focus */
  winsys_change_focus(thisWindow, window_focus_next_in_line(focus));
}

uint32_t
window_destroy(Window *thisWindow, bool unmap)
{
  Window *parent = thisWindow->parent;
  uint32_t retcode = RC_OK;

  /* Take care of the children first:  destroy them but no need to
     hide (unmap) them because unmapping their parent will take care
     of that. */
  {
    while (ADDRESS(thisWindow->children.next) != 
	   ADDRESS(&(thisWindow->children))) {
      Window *child = (Window *)(thisWindow->children.next);

      window_destroy(child, false);
    }
  }

  DEBUG(destroy) kprintf(KR_OSTREAM, "destroy(): hiding 0x%08x\n",
			 ADDRESS(thisWindow));

  if (unmap)
    window_hide(thisWindow);

  DEBUG(destroy) kprintf(KR_OSTREAM, "....    unlinking from sibchain.\n");

  link_Unlink(&thisWindow->sibchain);
  if(link_isSingleton(&parent->sibchain)){parent->sibchain.isHead=0;}
  
  if (thisWindow->type == WINTYPE_CLIENT ||
      thisWindow->type == WINTYPE_INVISIBLE)
    retcode = session_WinKill(thisWindow);

  if (retcode != RC_OK)
    return retcode;

  DEBUG(destroy) kprintf(KR_OSTREAM, "....     calling free.\n");
  free(thisWindow);

  return RC_OK;
}

void
window_set_name(Window *w, uint8_t *name)
{
  uint32_t len;

  window_clear_title(w);

  if (name == NULL)
    return;

  len = strlen(name);

  /* Don't copy more than MAX_TITLE characters */
  strncpy(w->name, name, MAX_TITLE);

  /* Ensure that result is null terminated */
  if (len >= MAX_TITLE)
    w->name[MAX_TITLE-1] = '\0';
}

/* Determine if all ancestors of specified Window are mapped. (Window
   contents can only be rendered if all its ancestors are mapped.) */
bool
window_ancestors_mapped(Window *w)
{
  Window *chk = w->parent;

  while(chk) {
    if (!chk->mapped)
      return false;
    chk = chk->parent;
  }
  return true;
}

/* Fills in supplied rect_t with coords of clipped rect in Root
   coords. Returns false if no intersection. */
bool
window_clip_to_ancestors(Window *w, rect_t *r)
{
  Window *par = w->parent;
  rect_t par_rect;
  rect_t ret_r;

  xform_win2rect(w, WIN2ROOT, &ret_r);

  while(par) {
    xform_win2rect(par, WIN2ROOT, &par_rect);
    if (!rect_intersect(&ret_r, &par_rect, &ret_r)) {
      r->topLeft.x = r->topLeft.y = 0;
      r->bottomRight.x = r->bottomRight.y = 0;
      return false;
    }
    par = par->parent;
  }

  *r = ret_r;
  return true;
}

/* 'region' is in Root coords */
void
window_draw(Window *thisWindow, rect_t region)
{
  Link *child;

  rect_t clip_root;
  rect_t win_root;

  if (!thisWindow->mapped)
    return;

  /* Determine max possible rect region by clipping thisWindow to all
     its ancestors. */
  window_clip_to_ancestors(thisWindow, &clip_root);

  /* Find intersection of that result and requested region */
  if (!rect_intersect(&region, &clip_root, &clip_root))
    return;

  /* Finally, make sure the result is actually bounded by thisWindow's
     dimensions! */
  xform_win2rect(thisWindow, WIN2ROOT, &win_root);
  if (!rect_intersect(&clip_root, &win_root, &clip_root))
    return;

  /* First render thisWindow */
  thisWindow->draw(thisWindow, clip_root);

  /* Next render all children */
  child = thisWindow->children.next;
  while(ADDRESS(child) != ADDRESS(&(thisWindow->children))) {
    Window *w = (Window *)child;

    window_draw(w, clip_root);
    child = child->next;
  }
}

void
window_show(Window *thisWindow)
{
  Window *parent;
  Window *draw_win = thisWindow; /* or else thisWindow's parent */
  clip_vector_t *exposed_areas;
  uint32_t u;

  if (thisWindow->type != WINTYPE_CLIENT && 
      thisWindow->type != WINTYPE_INVISIBLE)
    kdprintf(KR_OSTREAM, "Predicate failure in window_show().\n");

  if (thisWindow == NULL || thisWindow->parent == NULL)
    return;

  if (thisWindow->mapped)
    return;

  /* Set the mapping of thisWindow */
  thisWindow->mapped = true;
  parent = thisWindow->parent;

  if (parent->type == WINTYPE_DECORATION) {
    parent->mapped = true;
    draw_win = parent;
  }

  /* For nested windows need to run all the way up the ancestry chain
     to ensure that all ancestors are mapped. */
  if (!window_ancestors_mapped(thisWindow))
    return;

  DEBUG(map) kprintf(KR_OSTREAM, "window_show() calling get_subregions()");
  exposed_areas = window_get_subregions(draw_win, UNOBSTRUCTED);
  DEBUG(map) kprintf(KR_OSTREAM, "             returned.%d regions", 
		     exposed_areas->len);

  for (u = 0; u < exposed_areas->len; u++)
    window_draw(draw_win, exposed_areas->c[u].r);
}

void
window_initialize(Window *thisWindow, Window *parent, point_t origin, 
		  point_t size, void *session, uint32_t type)
{
  rect_t orig_size = { {0,0}, size };

  thisWindow->type = type;
  thisWindow->hasFocus = false;
  thisWindow->parent = parent;
  thisWindow->origin=origin;
  thisWindow->userClipRegion = orig_size;
  thisWindow->drawClipRegion = orig_size;
  thisWindow->size=size;
  thisWindow->session=session;
  thisWindow->mapped = false;

  /* assigned later */
  thisWindow->window_id = 0;
  thisWindow->deliver_mouse_event = NULL;
  thisWindow->deliver_key_event = NULL;
  thisWindow->draw = NULL;
  thisWindow->set_focus = NULL;
  thisWindow->render_focus = NULL;

  window_set_name(thisWindow, NULL);

  link_Init(&thisWindow->children, 0);
  link_Init(&thisWindow->sibchain, 0);

  /* Assuming that head is set to true the moment the first child is
     created(born) Not sure if this is the right thing to check to see
     if it is the first child */
  if (parent->children.isHead != 1) 
    link_Init(&parent->children,1);

  link_insertAfter(&parent->children,&thisWindow->sibchain);
}

Window *
window_create(Window *parent, point_t origin, point_t size, void *session,
	      uint32_t type)
{
  Window *thisWindow = malloc(sizeof(struct Window));

  window_initialize(thisWindow, parent, origin, size, session, type);

  return thisWindow;
}

void
window_dump(Window *w)
{
  kprintf(KR_OSTREAM, "=== Window Dump ===");
  kprintf(KR_OSTREAM, " ID = 0x%08x", w->window_id);
  kprintf(KR_OSTREAM, " origin (%d,%d)", w->origin.x, w->origin.y);
  kprintf(KR_OSTREAM, " size  %dx%d", w->size.x, w->size.y);
  kprintf(KR_OSTREAM, " mapped = %s", w->mapped ? "TRUE" : "FALSE");
  kprintf(KR_OSTREAM, " name   = \"%s\"", w->name);
  kprintf(KR_OSTREAM, " children = 0x%08x", ADDRESS(&(w->children)));
  kprintf(KR_OSTREAM, " sibchain = 0x%08x", ADDRESS(&(w->sibchain)));
  switch (w->type) {
  case WINTYPE_CLIENT:
    {
      kprintf(KR_OSTREAM, " type = CLIENT");
      break;
    }
  case WINTYPE_DECORATION:
    {
      kprintf(KR_OSTREAM, " type = DECORATION");
      break;
    }
  case WINTYPE_BUTTON:
    {
      kprintf(KR_OSTREAM, " type = BUTTON");
      break;
    }
  default:
    {
      kprintf(KR_OSTREAM, " type = ??");
      break;
    }
  }
  kprintf(KR_OSTREAM, "===================");
}

