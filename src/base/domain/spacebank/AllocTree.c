/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <stdlib.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>

#include "debug.h"
#include "misc.h"
#include "assert.h"
#include "constituents.h"
#include "spacebank.h"
#include "ObjSpace.h"
#include "AllocTree.h"
#include "AllocTree_Internals.h"
#include "malloc.h"
#include "Bank.h"

#if CND_DEBUG(tree)
#define VERBOSE
#endif

#undef TREE_STATIC_ALLOC /* NO STATIC ALLOCATION */

#ifdef TREE_STATIC_ALLOC

#define NUM_TREENODES 512

TREENODE static_treenodes[NUM_TREENODES]; /* added to free list on
					   * startup
					   */

#endif

void debug_print_node(TREENODE *node);

/* head of the free list -- a list of free nodes linked by their right
 * links.
 */
static TREENODE *freehead = 0;

TREENODE *
tree_newNode(TREEKEY key)
{
  TREENODE *newNode;
  
  if (freehead == 0) {
    TREENODE *nodes;
    uint32_t x;
    
    DEBUG(malloc) kdprintf(KR_OSTREAM, "Allocating more tree nodes\n");
#ifdef TREE_STATIC_ALLOC
    kpanic(KR_OSTREAM,
	     "Spacebank: out of statically allocated Treenodes!\n");
    return TREE_NIL;
#else
    
#define NUM_NODES (8)
    
    nodes = malloc(sizeof(TREENODE) * NUM_NODES);
    x = NUM_NODES;

    DEBUG(malloc) kdprintf(KR_OSTREAM, " ... got data at 0x%08x\n",nodes);

    if (!nodes) {
      kpanic(KR_OSTREAM,
	     "Spacebank: mallocing Treenodes failed!\n");
    }

    /* add all of the new nodes to the freelist */
    while (x) {
      nodes[--x].parent = freehead;
      freehead = &nodes[x];
    }
#undef NUM_NODES
#endif
  }
  newNode = freehead;
  freehead = newNode->parent;

  {
    /* initialize value */
    uint32_t x;
    
    newNode->left = TREE_NIL;
    newNode->right = TREE_NIL;
    newNode->parent = TREE_NIL;
    newNode->color = TREE_BLACK;

    newNode->value = key;

    /* initialize body */
    for (x = 0; x < SB_FRAMES_PER_TREENODE; x++) {
      newNode->body.map[x] = 0u;
#ifndef TREE_NO_TYPES
      newNode->body.type[x] = SBOT_INVALID;
#endif      
    }
  }
  return newNode;
  
}

int
tree_deleteNode(TREENODE *node)
{
  if (node != NULL && node != TREE_NIL) {

    node->parent = freehead; /* link it in to free list */
    freehead = node;        /* then put it as head.    */
    
    return 1;
    
  } else {
    return 0;
  }
    
}

/* allocTree functions */
static void
_allocTree_clearCachedInfo(TREE *target);
/*   Clears any cached info in /target/ -- should be called any time a
 * node is going to be deleted from a tree, as this can make the
 * cached info incorrect.
 */

uint32_t
allocTree_Init(TREE *tree)
{
  static uint32_t inited = 0;

  if (!inited && (inited = 1) == 1 ) { /* run only once */
    tree_init();

    TREE_NIL->value = 1u; /* to match no possible value */
  
#ifndef TREE_STATIC_ALLOC
    freehead = 0;
#else /*TREE_STATIC_ALLOC*/
    {
      /* set up free list */
      int x;
      
      freehead = &static_treenodes[0]; /* the first node it our head */
      
      for (x = 0; x < NUM_TREENODES - 1; x++) {
	static_treenodes[x].parent = &static_treenodes[x+1];
	/* each node points to the next one. */
      }
      
      /* special case -- last node points to 0 */
      static_treenodes[x].parent = 0;
    }
#endif
  }
  tree->root = TREE_NIL; /* initialize the tree */

  _allocTree_clearCachedInfo(tree);
  return 0;
}

static void
_allocTree_clearCachedInfo(TREE *target)
{
  if (target) {
    target->lastInsert = TREE_NIL;
    target->lastRemove = TREE_NIL;
  }
}

uint32_t
allocTree_insertOIDs(TREE *tree, uint8_t type, OID oid, uint32_t count)
{
  register uint32_t curMap;
  register uint32_t newMask;
  
  TREENODE *theNode = TREE_NIL;

  OID treeKey = FLOOR2(oid, SB_OBJECTS_PER_TREENODE);
  uint32_t offset = MOD2(oid / EROS_OBJECTS_PER_FRAME, SB_FRAMES_PER_TREENODE);
  uint32_t objNum = oid & (EROS_OBJECTS_PER_FRAME - 1);

  if (treeKey == tree->lastInsert->value)
    /*  note that if tree->lastInsert == TREE_NIL, lastInsert->value
     * will never match treeKey, so tree_find will be run..
     */
    theNode = tree->lastInsert;
  else
    theNode = tree_find(tree->root,treeKey);

  if (theNode == TREE_NIL) {
    theNode = tree_newNode(treeKey);

    /* initialized the node -- now add it to the tree */
    tree->root = tree_insert(tree->root, theNode);
  }

  tree->lastInsert = theNode; /* store it for next time */
  
#ifndef TREE_NO_TYPES
#ifndef NDEBUT
  /* make sure the types match */
  if (theNode->body.type[offset] != SBOT_INVALID
      && (theNode->body.type[offset] != type
	  && theNode->body.map[offset] != 0u
         )) {
    debug_print_node(theNode);
    kpanic(KR_OSTREAM,
	     "Spacebank: allocTree_insert at 0x"DW_HEX" got type "
	     "conflict.  Offset %u says \n"
	     "           type %u, but caller says type %s!\n",
	     DW_HEX_ARG(treeKey),
	     offset,
	     (uint32_t)theNode->body.type[offset],
	     type_name(type)
	    );
    return 0; /* RETURN -- "error" */
  }
#endif
#endif

  DEBUG(alloc)
    if (!valid_type(type)) {
      kpanic(KR_OSTREAM,
	     "Spacebank: allocTree_insert(0x"DW_HEX") got sent "
	     "invalid type %u\n",
	     DW_HEX_ARG(oid),
	     type
	     );
      return 0; /* RETURN -- "error" */
    }

  /*  DEBUG(alloc)*/ if (1) 
    if (count + objNum > objects_per_frame[type]) {
      debug_print_node(theNode);
      kpanic(KR_OSTREAM,
	     "Spacebank: allocTree_insert(0x"DW_HEX",%s) got bad count\n"
	     "           (objNum (%d) + count (%d) > "
	                                 "objects_per_frame[] (%d)\n",
	     DW_HEX_ARG(oid),
	     type_name(type),
	     objNum,
	     count,
	     objects_per_frame[type]
	    );
      return 0; /* RETURN -- "error" */
    }

  curMap = theNode->body.map[offset]; /* get it from the node */

  /* muck with it */
#ifndef TREE_NO_TYPES
  if (curMap == 0u) {
    /* re-type the node */
    theNode->body.type[offset] = type;

    curMap = 0u; /* currently redundant -- might change later */
    /* could possibly be object_map_mask, but that makes tests slow */
  }
#endif

  newMask = (((1u << count) - 1) << objNum);

  /* test whether there are any ones where the mask is */
  if ( (curMap & newMask ) ) {
    debug_print_node(theNode);
    kpanic(KR_OSTREAM,
	   "Spacebank: allocTree_insert(0x"DW_HEX") "
	   "attempted to exceed type frame count limit\n"
	   "curMap: 0x%04x, new mask: %04x full map: %04x\n",
	   DW_HEX_ARG(oid),
	   curMap, newMask, objects_map_mask[type]);
  }

  /*  kprintf(KR_OSTREAM,
	  "SpaceBank:  AllocTree for 0x"DW_HEX" frame 0x%02x old "
	  "count %d new count %d\n",
	  DW_HEX_ARG(treeKey),
	  (uint32_t)offset,
	  curCount,
	  curCount + count);
   */

  /* FIXME: faster to do it directly? */
  
  curMap |= newMask;
  
  theNode->body.map[offset] = curMap; /* write it to
				       * theNode.
				       */

  return 1; /* RETURN -- "done" */
}

uint32_t
allocTree_checkForOID(TREE *tree, OID oid)
{
  TREENODE *theNode;

  OID treeKey = FLOOR2(oid, SB_OBJECTS_PER_TREENODE);
  uint32_t offset = MOD2(oid / EROS_OBJECTS_PER_FRAME, SB_FRAMES_PER_TREENODE);
  uint32_t objNum = oid & (EROS_OBJECTS_PER_FRAME - 1);

  if (treeKey == tree->lastRemove->value)
    /*  note that if tree->lastRemove == TREE_NIL, lastInsert->value
     * will never match treeKey, so tree_find will be run..
     */
    theNode = tree->lastRemove;
  else 
    theNode = tree_find(tree->root,treeKey);

  if (theNode == TREE_NIL) {
    return 0;	
  }
  tree->lastRemove = theNode;

  return (theNode->body.map[offset] & (1 << objNum));
}

uint32_t
allocTree_removeOID(TREE *tree, struct Bank *bank, uint8_t type, OID oid)
{
  TREENODE *theNode;

  OID treeKey = FLOOR2(oid, SB_OBJECTS_PER_TREENODE);
  uint32_t offset = MOD2(oid / EROS_OBJECTS_PER_FRAME, SB_FRAMES_PER_TREENODE);
  uint32_t objNum = oid & (EROS_OBJECTS_PER_FRAME - 1);

  register uint32_t curMap;
  register uint32_t newMask;
  
  if (treeKey == tree->lastRemove->value)
    /*  note that if tree->lastRemove == TREE_NIL, lastInsert->value
     * will never match treeKey, so tree_find will be run..
     */
    theNode = tree->lastRemove;
  else
    theNode = tree_find(tree->root,treeKey);

  assert(theNode != NULL);
  
  /* See if the OID in question is in the tree: */
  if (theNode == TREE_NIL) {
    DEBUG(dealloc)
      kdprintf(KR_OSTREAM,
	       "allocTree_removeOID passed OID 0x"DW_HEX", "
	       "which does not have a treenode\n",
	       DW_HEX_ARG(oid));
    return 0; /* RETURN "object not within tree" */
  }

  tree->lastRemove = theNode; /* store it for next time */

  curMap = theNode->body.map[offset];
  
  if (curMap == 0) { /* empty */
    DEBUG(dealloc)
      kdprintf(KR_OSTREAM,
	       "allocTree_removeOID passed OID 0x"DW_HEX", "
	       "whose frame is not part of this tree\n",
	       DW_HEX_ARG(oid));
    return 0; /* RETURN "object not within tree" */
  }
#ifndef TREE_NO_TYPES
#ifndef NDEBUG
  if (theNode->body.type[offset] != type) {
    debug_print_node(theNode);
    kpanic(KR_OSTREAM,
	   "allocTree_removeOID passed OID 0x"DW_HEX", whose "
	   "type (%s)is not the same as the one passed in (%s)\n",
	   DW_HEX_ARG(oid),
	   theNode->body.type[offset],
	   type);
    return 0; /* RETURN "object has wrong type" */
  }
#endif
#endif

  newMask = (1 << objNum);
  
  if ( (curMap & newMask) != newMask) {
    DEBUG(dealloc)
      kdprintf(KR_OSTREAM,
	       "allocTree_removeOID passed OID 0x"DW_HEX", "
	       "who is not marked allocated in its map (0x%04x)\n",
	       DW_HEX_ARG(oid),
	       curMap);
    return 0; /* RETURN "object not allocated" */
  }

  DEBUG(dealloc)
    kprintf(KR_OSTREAM,
	    "Deallocating %s 0x"DW_HEX", whose frame has "
	     "map 0x%04x (before)\n",
	    type_name(type),
	    DW_HEX_ARG(oid),
	    curMap);

  curMap &= ~newMask;
  
  if (curMap == 0) {
    /* this part of the node is completely deallocated -- check if the
     * whole node is empty, too, and if so, delete it from the tree.
     */
    uint32_t x;

    DEBUG(dealloc)
      kprintf(KR_OSTREAM,
	      "Frame empty -- checking if everything is empty\n");

    /* invalidate the offset */
    theNode->body.map[offset] = 0u; 
#ifndef TREE_NO_TYPES
    theNode->body.type[offset] = SBOT_INVALID;
#endif

    /* check if we are empty */
    x = SB_FRAMES_PER_TREENODE;

    while (x > 0) {
      x--;
      if (theNode->body.map[x] != 0u) {
	x = SB_FRAMES_PER_TREENODE;
	break; /* BREAK "Nope, not empty yet!" */
      }
    }
    
    if (x == 0) {
      /* completely empty */

      DEBUG(dealloc)
	kprintf(KR_OSTREAM,
		"TREENODE empty -- deleting it\n");

      _allocTree_clearCachedInfo(tree); /* we are going to be deleting*/

      /* remove the node */
      tree->root = tree_remove(tree->root, theNode);

      /* since object pointed to by lastRemove is now ancient history,
	 nil that out: */
      tree->lastRemove = TREE_NIL;

      /* It may happen (has happened!) that this tree node we are
	 about to delete is also the most recently *inserted* node.
	 If so, we must nil that out too lest a deleted node get stuck
	 into the tree somehow. */
      if (tree->lastInsert == theNode)
	tree->lastInsert = TREE_NIL;
      
      /* and delete it */
      tree_deleteNode(theNode);
    }

    /* release the now empty frame */
    DEBUG(limit)
      kprintf(KR_OSTREAM,
	      "allocTree_removeOID unreserving frame for oid "
	      DW_HEX"\n",
	      DW_HEX_ARG(oid));
	      
    bank_UnreserveFrames(bank, 1);
    if (type == capros_Range_otNode)
      ob_ReleaseNodeFrame(bank, EROS_FRAME_FROM_OID(oid));
    else
      ob_ReleasePageFrame(bank, EROS_FRAME_FROM_OID(oid));
  } else {
    theNode->body.map[offset] = curMap; /*
					 * put in the new map
					 */
    DEBUG(dealloc)
      kprintf(KR_OSTREAM,
	      "Wrote new map 0x%04x\n",curMap);
  }

  return 1; /* RETURN "success" */
}

#ifdef NEW_DESTROY_LOGIC
uint32_t
allocTree_findOID(TREE *tree, OID *pOID, uint8_t *obType)
{
  TREENODE *curNode = tree->root;
  OID oid;
  uint32_t index;

  if (curNode == TREE_NIL)
    return 0;			/* nothing left */

  oid = curNode->value;

  for (index = 0; index < SB_FRAMES_PER_TREENODE; index++) {
    if ((curNode->body.type[index] != SBOT_INVALID) &&
	(curNode->body.map[index] != 0u)) {
      unsigned subOb;

#ifndef TREE_NO_TYPES
      *obType = curNode->body.type[index];
#endif

      for (subOb = 0; subOb < EROS_OBJECTS_PER_FRAME; subOb++) {
	uint8_t newMask = (1u << subOb);
	if (curNode->body.map[index] & newMask)
	  *pOID = curNode->value + (index * EROS_OBJECTS_PER_FRAME) + subOb;
	return 1;
      }
    }
  }

  kpanic(KR_OSTREAM,"Non-empty AllocTree has node with no allocations!\n",__LINE__);
  return 0;
}
#else
uint32_t
allocTree_IncrementalDestroy(TREE *toDie, OID *retFrame)
{
  if (toDie->root == TREE_NIL) {
    toDie->lastInsert = TREE_NIL;
    toDie->lastRemove = TREE_NIL;
    
    return 0; /* RETURN "there is no more left!" */
  } else {
    TREENODE *curNode = toDie->root;
    OID oid;
    uint32_t index;
    
    oid = curNode->value;
    for (index = 0; index < SB_FRAMES_PER_TREENODE; index++) {
      if (curNode->body.map[index] != 0u) {
	if (retFrame) *retFrame = oid + index * EROS_OBJECTS_PER_FRAME;
	oid++;
#ifndef TREE_NO_TYPES
	curNode->body.type[index] = SBOT_INVALID;
#endif
	curNode->body.map[index] = 0u;
	index++; /* increment past this one, as we know it is zero */
	break;
      }
    }

    /* skip past any empty ones after this one */
    while (index < SB_FRAMES_PER_TREENODE
	   && curNode->body.map[index] == 0u) {
      index++;
    }
    if (index == SB_FRAMES_PER_TREENODE) {
      /* this node is finished */
      /* we are going to be deleting */      
      _allocTree_clearCachedInfo(toDie); 
      toDie->root = tree_remove(toDie->root, curNode);

      /* SHAP: and delete it */
      tree_deleteNode(curNode);
    }
    return 1; /* RETURN "got some" */
  }  
}
#endif

uint32_t
allocTree_mergeTrees(TREE *dest, TREE *src)
{
  TREENODE *curNode = src->root;

  if (src == dest)
    kpanic(KR_OSTREAM,"AllocTree.c:%d: Merging identical trees!\n",__LINE__);

  _allocTree_clearCachedInfo(src); /* we are going to be deleting */
  
  while (curNode != TREE_NIL) {
    /*   loop through the tree, removing the root of /src/ and
     * inserting it's contents into /dest/
     */
    TREENODE *destNode;
    
    src->root = tree_remove(src->root,curNode);
    
    destNode = tree_find(dest->root,curNode->value);

    if (destNode == TREE_NIL) {
      /* not there - insert it */
      dest->root = tree_insert(dest->root,curNode);
    } else {
      /* there -- merge the old and new */
      uint32_t idx;

      for (idx = 0; idx < SB_FRAMES_PER_TREENODE; idx++) {
	if (curNode->body.map[idx] != 0) {
#ifdef PARANOID
	  if (destNode->body.map[idx] != 0) {
	    debug_print_node(curNode);
	    debug_print_node(destNode);
	    /* conflict */
	    kpanic(KR_OSTREAM,"Spacebank: treenode merge conflict\n");
	  }
#endif /*PARANOID*/	  
	  /* write in the data */
#ifndef TREE_NO_TYPES
	  destNode->body.type[idx] = curNode->body.type[idx];
#endif
	  destNode->body.map[idx] = curNode->body.map[idx];
	}
      }
      /* done merging, delete the node */
      tree_deleteNode(curNode);
    }

    curNode = src->root; /* reset curNode to the new root */
    
  } /* while (curNode != TREE_NIL) */

  /* the old tree is dead, long live the tree! */
  src->root = TREE_NIL;
  src->lastInsert = TREE_NIL;
  src->lastRemove = TREE_NIL;
      
  /* Having done the merge, our cached information about last
     insertion and removal may now be stale, but there is no point in
     chucking it.  Think of the spacebank clobber that just occurred
     as being "out of band" w.r.t. the normal sequence of allocations
     and deletions.  If we're right we win, and if we're wrong we cost
     ourselves a marginal compare, which is not a bad downside. */

  return 0;
}

void
debug_print_node(TREENODE *node)
{
  uint32_t i;
  kprintf(KR_OSTREAM,
	  "%sNode 0x%08x Parent 0x%08x Left 0x%08x Right %08x\n",
	  ((node->color == TREE_BLACK)?"Black  ":
	    (node->color == TREE_RED)?"Red    ": "BADCLR "),
	  node, node->parent, node->left, node->right);

  kprintf(KR_OSTREAM,
	  "Value: 0x%08x%08x\n",
	  DW_HEX_ARG(node->value));

#ifndef TREE_NO_TYPES
  kprintf(KR_OSTREAM,
	  "Type: ");

  for (i = 0; i < SB_FRAMES_PER_TREENODE; i++)
    kprintf(KR_OSTREAM, "   %02x", (uint32_t)node->body.type[i]);

  kprintf(KR_OSTREAM,"\n");
#endif	  

  kprintf(KR_OSTREAM,
	  "Map:  ");

  for (i = 0; i < SB_FRAMES_PER_TREENODE; i++)
    kprintf(KR_OSTREAM, " %04x", (uint32_t)node->body.map[i]);

  kprintf(KR_OSTREAM,"\n");
}

/* node comparision functions */
#ifdef NO_FAST_MACRO_COMPARE
static int
tree_compare(TREENODE *t0, TREENODE *t1)
{
  if (t0->value < t1->value)
    return -1;
  if (t0->value > t1->value)
    return 1;
  return 0;
}

static int
tree_compare_key(TREENODE *t0, TREEKEY key)
{
  /* NOTE: in order to be found correctly, /key/ *MUST* have
   *       zeros in the low bits.  The best way to do this is
   *       to AND it with ~(SB_OBJECTS_PER_TREENODE - 1) before
   *       passing it to any call that uses this routine.
   */
  if (t0->value < key)
    return -1;
  if (t0->value > key)
    return 1;
  return 0;
}
#else /* ! NO_FAST_MACRO_COMPARE */ 
/* use macro versions of the above for speed! */
#define tree_compare(t0,t1) (((t0)->value < (t1)->value)? -1 : \
                             (((t0)->value > (t1)->value)? 1 : 0))

#define tree_compare_key(t0,key) (((t0)->value < key)? -1 : \
                                  (((t0)->value > key)? 1 : 0))
#endif /* ! NO_FAST_MACRO_COMPARE */

#define ERROR_PRINTF(x) kdprintf x
#define ERR_FIL KR_OSTREAM

#define VERB_PRINTF(x) kprintf x
#define VERB_FIL KR_OSTREAM

#include <rbtree/tree_init.c>
#include <rbtree/tree_util.c>
#include <rbtree/tree_find.c>
#include <rbtree/tree_insert.c>
#include <rbtree/tree_validate.c>
#include <rbtree/tree_remove.c>
#include <rbtree/tree_contains.c>
#include <rbtree/tree_succ.c>
#include <rbtree/tree_pred.c>
#include <rbtree/tree_min.c>
#include <rbtree/tree_max.c>


