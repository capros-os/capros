/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* The RB-Tree implementation is based on one from the OpenCM code base */


/* Now that you've read the words from our sponsors, here's some stuff that
 * might be actually useful. This description assumes familarity with
 * LogDirectory.h, which defines the external interface.
 * 
 * The log directory keeps its information in a data structure called a
 * TreeNode (defined below). One of these is used for each OID recorded
 * in the directory. These nodes are organized two ways:
 *  (1) They are placed in an RB tree, indexed by OID.
 *  (2) All the Primary Locations for a given generation are linked together
 *      in a doubly-linked list headed in the generation_table, which has
 *      an entry for each generation. (Yes, there is a compile-time limit
 *      on the maximum number of generations that can be described in the
 *      directory.)
 *
 * The generation_table is an array. Its index is the highest generation
 * number seen minus the generation number of the entry as calculated by
 * get_generation_index(). In addition to the chain head for the generation
 * chain, the generation_table maintains the number of Primary Locations
 * in the generation, and a cursor used by ld_getNextObject().
 */

#include "kerninc/LogDirectory.h"

#define dbg_tree	0x1u
#define dbg_chain	0x2u
#define dbg_verbose	0x4u

/* Following should be an OR of some of the above */
#ifndef dbg_flags
#ifndef SELF_TEST
#define dbg_flags   ( 0u )
#else
#define dbg_flags   ( dbg_tree | dbg_chain )
#endif
#endif

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define FALSE false
#define TRUE true

#define TREE_RED 0
#define TREE_BLACK 1

typedef struct TreeNode {
  /* Data that describes the primary location of the object. */
  GenNum generation;
  ObjectDescriptor od;

  /* Data that describes the previous primary location of the object */
  uint8_t ppGenerationDelta; /* Difference between ppgeneration and
				the primary generation. If this field is
			        zero, there is no previous primary data. */
  LID ppLogLoc;              /* LID for previous location */

  /* Data for the RB tree */
  struct TreeNode *left;
  struct TreeNode *right;
  struct TreeNode *parent;
  int color;

  /* Data for the doubly linked list */
  struct TreeNode *prev;
  struct TreeNode *next;
} TreeNode;

unsigned long numLogDirEntries;
TreeNode * logDirNodes = NULL;

#define TREE_NIL (&log_directory_nil_node)
TreeNode log_directory_nil_node = {
  0, { 0, 0, 0, 0, 0 },    0, 0,
  TREE_NIL, TREE_NIL, TREE_NIL, TREE_BLACK,
  NULL, NULL};

TreeNode *free_list = NULL;

typedef struct TreeHead {
  TreeNode *root;
} TreeHead;


/** Objects known to the log directory.

    When looking for a log location for a journal write, check the generation
    of the primary location for the entry. If it is the working generation,
    use the data from the previous primary. If generation of that entry is
    a migrated and commited generation, use the home location.
 */
TreeHead log_directory = {TREE_NIL};

/** Headers for lists of all TreeNodes of a given generation.

    In addition to the tree we keep for locating the objects by OID,
    each of the logDirNodes is kept in a linked list by generation
    to enable us to quickly delete them when we overlay an old generation
    with more recent data and to scan them for writing the checkpoint
    directory and for migration.

    If a generation older than highest_generation - LD_MAX_GENERATIONS 
    is found, that is an error.
 */
#define LD_MAX_GENERATIONS 15
typedef struct {
  TreeNode *head;
  TreeNode *cursor;
  unsigned long count;
} GT;

GT generation_table[LD_MAX_GENERATIONS];
GenNum highest_generation = 0;
int log_entry_count = 0; /** Number of allocated TreeNodes */
GenNum last_retired_generation = 0;

/** Allocate a TreeNode from the free list.

    @return The allocated tree node (or assert FALSE).
*/
static TreeNode *
alloc_node(void) {
  TreeNode *tn = free_list;
  if (NULL == tn) {
    printf("Out of log directory nodes\n");
    assert(FALSE);
  }
  free_list = tn->next;
  log_entry_count++;
  return tn;
}

/** Return a TreeNode to the free list.

    @param[in] tn The TreeNode to release.
*/
static void
free_node(TreeNode *tn) {
  DEBUG(tree) {
    tn->left = tn->right = tn->parent = tn->prev = NULL;
    tn->generation = -1;
    assert(tn != generation_table[0].head);
  }
  tn->next = free_list;
  free_list = tn;
  assert(log_entry_count > 0);
  --log_entry_count;
}


/** Return the index in the generation table for a given generation.

    @param[in] generation The generation for which the index is wanted
    @return The index for the generation table.
*/
static int
get_generation_index(GenNum generation) {
  GenNum gdelta = highest_generation - generation;
  if (generation > highest_generation || gdelta >= LD_MAX_GENERATIONS)
    fatal("Invalid generation %d\n", generation);
  return gdelta;
}


/** Chain a node into the generation chain.

    @param[in] n The node to chain.
*/
static void
chain_node(TreeNode *n) {
  int gti = get_generation_index(n->generation);
  generation_table[gti].count++;
  TreeNode * t = generation_table[gti].head;
  if (NULL == t) {	// add to an empty list
    generation_table[gti].head = n;
    n->next = n;
    n->prev = n;
  } else {
    n->next = t->next;
    t->next->prev = n;
    n->prev = t;
    t->next = n;
  }
}

#if (dbg_flags & dbg_chain)
/** Sanity check the generation chain. */
static void
chains_validate(void) {
  int total_count = 0;
  int gti;
  for (gti=0; gti<LD_MAX_GENERATIONS; gti++) {
    TreeNode * last = generation_table[gti].head;
    TreeNode * cursor = generation_table[gti].cursor;
    bool cursor_found = (NULL == cursor);
    if (NULL != last) {
      int chain_count = 0;
      TreeNode * x = last->next;
      for (;;) {
        if (!cursor_found) cursor_found = (x == cursor);
        chain_count++;
        TreeNode * y = x->next;
        assert(y->prev == x);
        if (x == last) break;
        x = y;
      }
      assert(chain_count == generation_table[gti].count);
      total_count += chain_count;
    }
    assert(cursor_found);
  }
  if (total_count != log_entry_count) {
    dprintf(true, "%d TreeNodes allocated, %d in chains\n",
            log_entry_count, total_count);
  }
}
#endif


/** Remove a node from the generation chain.

    @param[in] n The node to chain.
*/
static void
unchain_node(TreeNode *n) {
  int gti = get_generation_index(n->generation);
  assert(generation_table[gti].count > 0);
  generation_table[gti].count--;
  GT *gte = &generation_table[gti];
  if (gte->head == n) gte->head = n->prev;
  if (gte->head == n) gte->head = NULL;
  if (gte->cursor == n) gte->cursor = n->next;
  n->prev->next = n->next;
  n->next->prev = n->prev;
  n->next = n->prev = n;
  if (gte->cursor == gte->head) gte->cursor = NULL;
}


/** Compare two object descriptors for tree operations.

    @param[in] od1 The first Object Descriptor to compare
    @param[in] od2 The second Object Descriptor to compare
    @return negative if od1 is less than od2, 0 if they are equal,
            and positive if od1 is greater than od2.
*/
inline static int
comp(const ObjectDescriptor *od1, const ObjectDescriptor *od2) {
  if (od1->oid < od2->oid) return -1;
  else if (od1->oid == od2->oid) return 0;
  return 1;
}

#if (dbg_flags & dbg_tree)

/** Recursively validate the substructure of a RB tree - for debugging.

    @param[in] node The top node of the (sub)tree to be checked.
    @return TRUE if the structure is valid, FALSE otherwise.
*/
static bool
tree_validate_recurse(TreeHead *tree, TreeNode *node) {
  if (node == TREE_NIL)
    return TRUE;
  
  if (node != NULL) {
    if (node->parent == 0) {
      printf("node 0x%08x parent is zero!\n", node);
      return FALSE;
    }
    if (node->left == 0) {
      printf("node 0x%08x parent is zero!\n", node);
      return FALSE;
    }
    if (node->right == 0) {
      printf("node 0x%08x parent is zero!\n", node);
      return FALSE;
    }
  }

  if (node == tree->root && node->parent != TREE_NIL) {
    printf("tree 0x%08x parent 0x%08x not nil!\n",
	   tree, node->parent);
    return FALSE;
  }
  
  if (node != tree->root && node->parent == TREE_NIL) {
    printf("non-root parent nil!\n");
    return FALSE;
  }

  if (node->left != TREE_NIL && node->left->parent != node) {
    printf("left child's parent!\n");
    return FALSE;
  }

  if (node->left != TREE_NIL && (comp(&node->left->od, &node->od) >= 0)) {
    printf("left child's value. node 0x%08x nd->l 0x%08x!\n",
	    node, node->left);
    return FALSE;
  }
  
  if (node->right != TREE_NIL && node->right->parent != node) {
    printf("right child's parent!\n");
    return FALSE;
  }

  if (node->right != TREE_NIL && (comp(&node->right->od, &node->od) <= 0)) {
    printf("right child's value. node 0x%08x nd->r 0x%08x!\n",
	    node, node->right);
    return FALSE;
  }

  if ( tree_validate_recurse(tree, node->left) == FALSE )
    return FALSE;
  
  return tree_validate_recurse(tree, node->right);
}


/** Validate the structure of a RB tree - for debugging.

    @param[in] node The top node of the (sub)tree to be checked.
    @return TRUE if the structure is valid, FALSE otherwise.
*/
static bool
tree_validate(TreeHead *tree, TreeNode *node) {
#if (dbg_flags & dbg_chain)
  chains_validate();
#endif
  return tree_validate_recurse(tree, node);
}
#endif /* dbg_tree */

/** Rotate the nodes in the tree clockwise to balance the tree and
    maintain the assertion that no child of a red node is red.

    The left descendent of the node to be rotated (y) moves to the
    place of y in the tree, while y moves to be its right child.

    @param[in] tree The top node of the tree.
    @param[in] y The node to be rotated right.
*/
static void
rb_tree_rightrotate(TreeHead *tree, TreeNode *y) {
  TreeNode *x = y->left;

  assert(x != TREE_NIL);	/* according to CLR */

  y->left = x->right;
  if (x->right != TREE_NIL)
    x->right->parent = y;

  if (x != TREE_NIL) x->parent = y->parent;
  if (y->parent == TREE_NIL)
    tree->root = x;
  else if (y == y->parent->right)
    y->parent->right = x;
  else
    y->parent->left = x;

  x->right = y;
  if (y != TREE_NIL) y->parent = x;

  assert(tree->root->parent == TREE_NIL);
}

/** Rotate the nodes in the tree counterclockwise to balance the tree and
    maintain the assertion that no child of a red node is red.

    The right descendent of the node to be rotated (x) moves to the
    place of x in the tree, while x moves to be its left child.

    @param[in] tree The top node of the tree.
    @param[in] x The node to be rotated right.
*/
static void
rb_tree_leftrotate(TreeHead *tree, TreeNode *x) {
  TreeNode *y = x->right;

  assert(y != TREE_NIL);	/* according to CLR */

  x->right = y->left;
  if (y->left != TREE_NIL)
    y->left->parent = x;

  if (y != TREE_NIL) y->parent = x->parent;
  if (x->parent == TREE_NIL)
    tree->root = y;
  else if (x == x->parent->left)
    x->parent->left = y;
  else
    x->parent->right = y;

  y->left = x;
  if (x != TREE_NIL) x->parent = y;

  assert(tree->root->parent == TREE_NIL);
}

/** Fix the coloring of the tree.

    Climb the tree from the specified node an fix the colors,
    rotating the tree as necessary.

    @param[in] tree The top node of the tree.
    @param[in] x The node to start climbing from.
*/
static void
tree_insert_fixup(TreeHead *tree, TreeNode *x) {
  assert(TREE_NIL->color == TREE_BLACK);
  
  while (x != tree->root && x->parent->color == TREE_RED) {
    if (x->parent == x->parent->parent->left) {
      TreeNode *y = x->parent->parent->right;
      if (y->color == TREE_RED) {
	x->parent->color = TREE_BLACK;
	y->color = TREE_BLACK;
	x->parent->parent->color = TREE_RED;
	x = x->parent->parent;
      }
      else {
	if (x == x->parent->right) {
	  x = x->parent;
	  rb_tree_leftrotate(tree, x);
	}
	x->parent->color = TREE_BLACK;
	x->parent->parent->color = TREE_RED;
	rb_tree_rightrotate(tree, x->parent->parent);
      }
    }
    else {
      TreeNode *y = x->parent->parent->left;
      if (y->color == TREE_RED) {
	x->parent->color = TREE_BLACK;
	y->color = TREE_BLACK;
	x->parent->parent->color = TREE_RED;
	x = x->parent->parent;
      }
      else {
	if (x == x->parent->left) {
	  x = x->parent;
	  rb_tree_rightrotate(tree, x);
	}
	x->parent->color = TREE_BLACK;
	x->parent->parent->color = TREE_RED;
	rb_tree_leftrotate(tree, x->parent->parent);
      }
    }
  }
  assert(TREE_NIL->color == TREE_BLACK);
}

/** Do a simple insert of a node in the tree.

    After the simple insert, the tree may be in an invalid state.
    Use tree_insert_fixup to restore the tree to a valid state.

    Note that if there is an existing node with the same OID and type,
    that node will be update and no new node will be inserted.

    @param[in] tree The top node of the tree.
    @param[in] od The object information to insert.
    @param[in] generation The generation of the object.
    @return A pointer to the node inserted, or NULL if an existing
    node was updated.
*/


static TreeNode *
binary_insert(TreeHead *tree, const ObjectDescriptor *od, GenNum generation) {
#if (dbg_flags & dbg_tree)
  int whichcase;
#endif

  TreeNode *y = TREE_NIL;
  TreeNode *x = tree->root;

#if (dbg_flags & dbg_tree)
  assert( tree_validate(tree, tree->root) );
#endif
  
  while (x != TREE_NIL) {

    assert (x->left == TREE_NIL || x->left->parent == x);
    assert (x->right == TREE_NIL || x->right->parent == x);

    y = x;
    if (comp(od, &x->od) == 0) {
      DEBUG(verbose) {
	printf("Update OID %lld in node 0x%08x\n", od->oid, x);
      }
      unchain_node(x);
      /* If same object and useful for journalize write */
      if (generation > x->generation
	  && x->generation > last_retired_generation
	  && x->od.allocCount == od->allocCount
	  && x->od.callCount == od->callCount
	  && x->od.type == od->type) {
	/* Save as previous location */
	assert(generation - x->generation < (uint8_t)0xff);
	x->ppGenerationDelta = generation - x->generation;
	x->ppLogLoc = x->od.logLoc;
      } else {
	x->ppGenerationDelta = 0;
      }
      x->od = *od;
      x->generation = generation;
      chain_node(x);
#if (dbg_flags & dbg_tree)
      assert (tree_validate(tree, tree->root) );
#endif
      return NULL;
    } else if (comp(od, &x->od) < 0) { /*if ( CMP(tree,z,x) < 0 ) { */
      x = x->left;
    }
    else {
      x = x->right;
    }
  }

#if (dbg_flags & dbg_tree)
  assert( tree_validate(tree, tree->root) );
#endif
  TreeNode *tn = alloc_node();
  tn->od = *od;
  tn->left = TREE_NIL;
  tn->right = TREE_NIL;
  tn->parent = TREE_NIL;
  tn->generation = generation;
  tn->ppGenerationDelta = 0;
  chain_node(tn);

  tn->parent = y;

  DEBUG(verbose) {
    printf("Insert OID %lld node 0x%08x into 0x%08x\n", tn->od.oid, tn, tree);
  }
  
  if (y == TREE_NIL) {
#if (dbg_flags & dbg_tree)
    whichcase = 1;
#endif
    tree->root = tn;
  }
  else {
#if (dbg_flags & dbg_tree)
    whichcase = 2;
#endif
    if (comp(&tn->od, &y->od) < 0) { /*if ( CMP(tree, z, y) < 0 ) { */
      y->left = tn;
#if (dbg_flags & dbg_tree)
      assert( tree_validate(tree, tree->root) );
#endif
    }
    else {
      y->right = tn;
#if (dbg_flags & dbg_tree)
      assert( tree_validate(tree, tree->root) );
#endif
    }
    assert (y->left == TREE_NIL || y->left->parent == y);
    assert (y->right == TREE_NIL || y->right->parent == y);
  }

  assert(tree->root->parent == TREE_NIL);

#if (dbg_flags & dbg_tree)
  if ( tree_validate(tree, tree->root) == FALSE ) {
    printf("Bad post-insert validation, case %d\n", whichcase);
    assert (FALSE);
  }
#endif
  return tn;
}


/** Find the smallest entry below a given node.
    
    @param[in] node The node to search from.
    @return A pointer to the smallest node or NULL.
*/
static TreeNode *
rbtree_do_min(TreeNode *node) {
  if (node == TREE_NIL)
    return TREE_NIL;
  
  while (node->left != TREE_NIL)
    node = node->left;

  return node;
}


/** Find the smallest entry larger than a given node.
    
    @param[in] node The node to search from.
    @return A pointer to the successor node or NULL.
*/static TreeNode *
rbtree_succ(TreeNode *x) {
  TreeNode *y;

  if (x == TREE_NIL)
    return TREE_NIL;
  
  if (x->right != TREE_NIL)
    return rbtree_do_min(x->right);

  y = x->parent;
  
  while (y != TREE_NIL && x == y->right) {
    x = y;
    y = y->parent;
  }

  return y;
}


/** Fix the tree structure after removing a node.
    
    @param[in] tree The head of the tree to fix.
    @param[in] x A child of the node being deleted.
    @return A pointer to the smallest node or NULL.
*/
static void
remove_fixup(TreeHead *tree, TreeNode *x) {
  assert(TREE_NIL->color == TREE_BLACK);

  if (x->parent == TREE_NIL)	/* deleted last node in tree */
    return;
  
  while ((x != tree->root) && (x->color == TREE_BLACK)) {
    /* MacManis checks x == nilnode && x.parent.left == null OR */
    if (x == x->parent->left) {
      TreeNode *w = x->parent->right;
      assert(w != TREE_NIL);

      if (w->color == TREE_RED) {
	w->color = TREE_BLACK;
	x->parent->color = TREE_RED;
	rb_tree_leftrotate(tree, x->parent);
	w = x->parent->right;
      }

      if ((w->left->color == TREE_BLACK) && (w->right->color == TREE_BLACK)) {
	w->color = TREE_RED;
	x = x->parent;		/* move up the tree */
      }
      else {
	if (w->right->color == TREE_BLACK) {
	  w->left->color = TREE_BLACK;
	  w->color = TREE_RED;
	  rb_tree_rightrotate(tree, w);
	  w = x->parent->right;
	}

	w->color = x->parent->color;
	x->parent->color = TREE_BLACK;
	w->right->color = TREE_BLACK;
	rb_tree_leftrotate(tree, x->parent);
	x = tree->root;
      }
    }
    else {
      TreeNode *w = x->parent->left;

      if (w->color == TREE_RED) {
	w->color = TREE_BLACK;
	x->parent->color = TREE_RED;
	rb_tree_rightrotate(tree, x->parent);
	w = x->parent->left;
      }

      if ((w->right->color == TREE_BLACK) && (w->left->color == TREE_BLACK)) {
	w->color = TREE_RED;
	x = x->parent;
      }
      else {
	if (w->left->color == TREE_BLACK) {
	  w->right->color = TREE_BLACK;
	  w->color = TREE_RED;
	  rb_tree_leftrotate(tree, w);
	  w = x->parent->left;
	}
	w->color = x->parent->color;
	x->parent->color = TREE_BLACK;
	w->left->color = TREE_BLACK;
	rb_tree_rightrotate(tree, x->parent);
	x = tree->root;
      }
    }
  }
  
  x->color = TREE_BLACK;
  assert(tree->root->parent == TREE_NIL);
  assert(TREE_NIL->color == TREE_BLACK);
}


#if (dbg_flags & dbg_tree)
/** Test if a (sub)tree contains a node.
    
    @param[in] tree The node to search from.
    @param[in] node The node to search for.
    @return TRUE if the tree contains the node, otherwise FALSE.
*/
static bool
rbtree_do_contains(TreeNode *tree, TreeNode *node) {
  if (tree == TREE_NIL)
    return false;

  if (tree == node)
    return true;

  assert (tree->left != tree);
  assert (tree->right != tree);
  
  if ( rbtree_do_contains(tree->left, node) )
    return true;
  
  return rbtree_do_contains(tree->right, node);
}


/** Test if a tree contains a node.
    
    @param[in] tree The tree head for the tree.
    @param[in] node The node to search for.
    @return TRUE if the tree contains the node, otherwise FALSE.
*/
static bool
rbtree_contains(TreeHead *tree, TreeNode *node) {
  return rbtree_do_contains(tree->root, node);
}
#endif


/** Remove a node from a tree.

    @param[in] tree The tree head for the tree.
    @param[in] node The node to remove.
*/
static void
tree_remove_node(TreeHead * tree, TreeNode *z) {
#if (dbg_flags & dbg_tree)
  int whichcase = 0;
#endif
  
  TreeNode *y = TREE_NIL;
  TreeNode *x = TREE_NIL;
  
  assert(tree->root->parent == TREE_NIL);
  assert(TREE_NIL->color == TREE_BLACK);

  DEBUG(verbose) {
    printf("Remove node 0x%08x from 0x%08x\n", z, tree);
  }

#if (dbg_flags & dbg_tree)
  if ( tree_validate(tree, tree->root) == FALSE ) {
    printf("Bad pre-remove validation, case 0x%08x\n", whichcase);
    assert (FALSE);
  }
#endif

  assert (z != TREE_NIL);

  if (z->left == TREE_NIL || z->right == TREE_NIL) {
#if (dbg_flags & dbg_tree)
    whichcase |= 0x1;
#endif
    /* Node to delete has only one child or none; can just splice it */
    /* out. */
    y = z;
  }
  else {
    /* Node to delete has two children.  Find it's suceessor and */
    /* splice successor in in place of deleted node. */
    y = rbtree_succ(z);
  }

  assert(y != TREE_NIL);

  /* We now know that y has at most one child.  Make x point to that */
  /* child */

  x = (y->left != TREE_NIL) ? y->left : y->right;

  x->parent = y->parent;	/* OKAY if X is TREE_NIL per CLR p. 273 */

  /* If y was the root, have to update the tree. */
  if (y->parent == TREE_NIL) {
#if (dbg_flags & dbg_tree)
    whichcase |= 0x10;
#endif
    tree->root = x;
  }
  else {
#if (dbg_flags & dbg_tree)
    whichcase |= 0x20;
#endif
    if (y == y->parent->left) {
      y->parent->left = x;
    }
    else {
      y->parent->right = x;
    }
  }

#if (dbg_flags & dbg_tree)
  if ( rbtree_contains(tree, y) ) {
    printf("x=0x%08x, y=0x%08x, z=0x%08x tree=0x%08x\n", x,
	   y, z, tree);
    printf("Deleted node 0x%08x still referenced before fixup!\n", y);
    assert(FALSE);
  }
#endif


  /* Until I actually understand the fixup transformation, do this */
  /* just as the book would have us do and then fix the fact that the */
  /* wrong thing was deleted after the fact. */
  
  {
    ObjectDescriptor zvalue = z->od;	/* SHAP */
    GenNum generation = z->generation;

    if (y != z) {
      unchain_node(z);  /* Remove z from the generation chain */
      z->od = y->od;
      z->generation = y->generation;
      chain_node(z);
    }
    if ( y->color == TREE_BLACK )
      remove_fixup(tree, x);

#if (dbg_flags & dbg_tree)
    if ( rbtree_contains(tree, (y!=z) ? y : z) )
      printf("Deleted znode 0x%08x still referenced by after fixup!\n", z);
    
    if ( tree_validate(tree, tree->root) == FALSE ) {
      printf("Bad post-remove validation, case 0x%08x\n",  whichcase);
      assert (FALSE);
    }
#endif

    if (y != z) {
#if (dbg_flags & dbg_tree)
      if ( rbtree_contains(tree, y) ) {
	printf("Deleted ynode (post fixup) 0x%08x still referenced "
	       "by after fixup!\n", z);
	assert(FALSE);
      }
#endif
    
      /* The tree is now correct, but for the slight detail that we have
       * deleted the wrong node.  It needed to be done that way for
       * remove_fixup to work.  At this point, Y is unreferenced, and
       * Z (the node we intended to delete) is firmly wired into the
       * tree.  Put Y in in Z's place and restore the old value to Z:
       *
       * At this point, Y and Z both have the correct bodies.  We need
       * to restore the correct key to Z, and swap the pointers
       * around so that Y gets inserted in place of Z.
       *
       * restore Z's value, which we smashed above.  */
      unchain_node(z);
      z->od = zvalue;
      z->generation = generation;
      chain_node(z);
      
      y->parent = z->parent;
      y->left = z->left;
      y->right = z->right;
      y->color = z->color;
      
      if (y->parent != TREE_NIL) {
	if (y->parent->left == z)
	  y->parent->left = y;
	else
	  y->parent->right = y;
      }
      if (y->left != TREE_NIL)
	y->left->parent = y;
      if (y->right != TREE_NIL)
	y->right->parent = y;
      
      if (tree->root == z)
	tree->root = y;
      
      assert (TREE_NIL->color == TREE_BLACK);
    }      
  }
#if (dbg_flags & dbg_tree)
  if ( tree_validate(tree, tree->root) == FALSE ) {
    printf("Bad post-remove validation, case 0x%08x\n", whichcase);
    assert (FALSE);
  }
  if ( rbtree_contains(tree, z) ) {
    printf("Deleted znode (post fixup) 0x%08x still "
	   "referenced by after fixup!\n", z);
    assert(FALSE);
  }
#elif (dbg_flags & dbg_chain)
  chains_validate();
#endif
  unchain_node(z);
  free_node(z);
  assert (TREE_NIL->color == TREE_BLACK);
}


/** Find a node in the tree.
    
    @param[in] tree The tree head for the tree.
    @param[in] oid The object ID to find.
    @return A pointer to the tree node for oid, or NULL.
*/
static TreeNode *
find_node(TreeHead *directory, OID oid) {
  TreeNode *n = directory->root;
  
  while (n != TREE_NIL) {
    if (oid == n->od.oid) {
      return n;
    }
    else if (oid < n->od.oid) {
      n = n->left;
    } else {
      n = n->right;
    }
  }
  return NULL;
}
/** Record the location of an object.

    The call includes the generation number so it may be used during
    restart.

    @param[in] od The Object Descriptor for the object.
    @param[in] generation The log generation of the object.
*/
void ld_recordLocation(const ObjectDescriptor *od, GenNum generation) {
  assert(TREE_NIL->color == TREE_BLACK);
  TreeHead *tree = &log_directory;
  
  if (generation > highest_generation) {
    if (0 == highest_generation) {
      /* Startup. Initialize the generation table */
      int i;
      for (i=0; i<LD_MAX_GENERATIONS; i++) {
	generation_table[i].head = NULL;
	generation_table[i].cursor = NULL;
	generation_table[i].count = 0;
      }
      /* Chain the TreeNodes on the free list */
      printf("numLogDirEntries=%d\n", numLogDirEntries);
      log_entry_count = numLogDirEntries;
      for (i=0; i<numLogDirEntries; i++) {
	free_node(&logDirNodes[i]);
      }
      assert(0 == log_entry_count);
    } else {
      /* Move the generation table to accomodate the new generation */
      GenNum move_size = generation - highest_generation;
      int ms = (move_size > LD_MAX_GENERATIONS
		? LD_MAX_GENERATIONS : move_size);
      int i;
      for (i=LD_MAX_GENERATIONS-1; i>=LD_MAX_GENERATIONS-ms; i--) {
	/* If there are any logDirNodes in these generations, it is an error */
        if (generation_table[i].head != NULL)
          fatal("More than %d generations!\n", LD_MAX_GENERATIONS);
      }
      for (; i>=0; i--) {
	/* These generations are kept lower in the table */
	generation_table[i+ms] = generation_table[i];
      }
      for (i=ms-1; i>=0; i--) {
	/* And set the earliest entries to NULL */
	generation_table[i].head = NULL;
	generation_table[i].cursor = NULL;
	generation_table[i].count = 0;
      }
    }
    highest_generation = generation;
  }
  
  TreeNode *tn = binary_insert(tree, od, generation);
  
  if (NULL == tn) return;
  
  tn->color = TREE_RED;
  
  tree_insert_fixup(tree, tn);
  
  tree->root->color = TREE_BLACK;
  assert(tree->root->parent == TREE_NIL);
  assert(TREE_NIL->color == TREE_BLACK);
  
#if (dbg_flags & dbg_tree)
  if ( !tree_validate(tree, tree->root) ) {
    printf("Tree bad after ld_recordLocation()\n");
    assert(FALSE);
  }
#elif (dbg_flags & dbg_chain)
  chains_validate();
#endif
}


/** Find an object in the directory.

    This routine will return primary location information for objects.

    @param[in] oid The object ID to be located.
    @return A pointer to the ObjectDescriptor for the object or NULL if the
            object is not in the log.
*/
const ObjectDescriptor *ld_findObject(OID oid) {
  TreeNode *n = find_node(&log_directory, oid);
  if (NULL != n) return &n->od;
  return NULL;
}


/** Find an object for a journalize write.

    This routine will the most recent location LID for the object
    if and only if: 
      (1) It is older than the given generation.
      (2) It is younger than the most recent generation specified in a
          call to ld_generationRetired().
    If there is no location meeting these requirements, it will return NULL.

    @param[in] oid The object ID to be located.
    @param[in] generation The generation the object must be older than.
    @return A pointer to the LID for the object or NULL if the object
            is not in the log, the log entry is younger or equal in age
	    to generation, or the entry is older than the most recent
	    retired generation. This pointer will be good until
	    a change is made to the log directory, adding, deleting, or
	    modifing an entry, or deleting a generation.
*/
const LID *
ld_findObjectForJournal(OID oid, GenNum generation) {
TreeNode *n = find_node(&log_directory, oid);
  if (NULL == n) return NULL;
  if (generation > n->generation && n->generation > last_retired_generation) {
    return &n->ppLogLoc;
  }
  if (0 == n->ppGenerationDelta) return NULL; /* no pp data */
  GenNum nppgen = n->generation - n->ppGenerationDelta;
  if (generation > nppgen && nppgen > last_retired_generation) {
    return &n->ppLogLoc;
  }
  return NULL;
}


/** Start the scan of a generation.

    This routine sets the cursor used by ld_findNextObject to the 
    first object of the generation. It may be used to start a scan of 
    all objects in a generation. ld_findNextObject returns successive
    ObjectDescriptors for the generation. There may be up to one scan in
    progress at any time for any particular generation. Only objects
    whose primary location is in the given generation will be returned.

    Note the the order of objects in a generation is undefined. If it needs
    to be defined for some reason, like optimizing migration, then that
    need will be an additional constraint on the implementation of the
    object directory or the use of the returned values.

    @param[in] generation The generation number to scan.
*/
void ld_resetScan(GenNum generation) {
  int gti = get_generation_index(generation);
  GT *gte = &generation_table[gti];
  gte->cursor = gte->head;
}

/** Find the next object of a generation.

    This routine continues the scan of all objects in a generation.
    See ld_findFirstObject for more information.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the next object in a generation scan.
*/
const ObjectDescriptor *ld_findNextObject(GenNum generation) {
  int gti = get_generation_index(generation);
  GT *gte = &generation_table[gti];
  if (NULL == gte->cursor)
    return NULL;
  ObjectDescriptor *od = &gte->cursor->od;
  if (gte->cursor->next == gte->head) {
    gte->cursor = NULL;
  } else {
    gte->cursor = gte->cursor->next;
  }
  return od;
}

/** Remove all the objects in a generation, and all earlier generations,
    from the Object Directory.

    Note: This routine may need to be executed in smaller pieces to meet
    real-time requirements.

    @param[in] generation The generation to clear.
*/
void ld_clearGeneration(GenNum generation) {
  int gti = get_generation_index(generation);
  for (; gti < LD_MAX_GENERATIONS; gti++) {
    TreeNode *tn = generation_table[gti].head;
    TreeHead *tree = &log_directory;
    
    if (NULL == tn) continue;
    while (tn->next != tn) {
      tree_remove_node(tree, tn->next);
    }
    tree_remove_node(tree, tn);
    assert(generation_table[gti].head == NULL);
    assert(generation_table[gti].cursor == NULL);
  }
}
 


/** Inform the Log Directory that a generation has been migrated, and a
    checkpoint written which records that fact.

    The Log Directory is free to re-use entries referring to retired
    generations should space be needed for recording more recent objects
    in the directory.

    @param[in] generation The generation which has been retired.
*/
void
ld_generationRetired(GenNum generation) {
  assert(generation >= last_retired_generation);
  last_retired_generation = generation;
}


/** Return the number of available log directory entries.

    A directory entry is considered available unless it is used
          to record a member of a generation newer than
          the specified generation.

    @param[in] generation The youngest generation to include in the
               free count. All older generations will be included in the
	       count.
    @return The number of available directory entries.
*/
unsigned long
ld_numAvailableEntries(GenNum generation) {
  int i;
  unsigned long count = numLogDirEntries;
  for (i = 0; i < LD_MAX_GENERATIONS; i++) {
    if (highest_generation - i <= generation)
      break;
    count -= generation_table[i].count;
  }
  return count;
}


/** Return the number of OIDs in the working generation.

    The Log Directory considers the highest numbered generation to be
    the working generation. After a demarcation event, the checkpoint
    logic stabilizes the working generation. Any objects which are 
    altered during this time period are part of the next generation.
    When the stabilization event occurs which commits the checkpoint,
    these objects become logically part of the new working generation.
    The Log Directory will not notice that this event has occured until
    the first entry for the new generation is recorded with
    ld_recordLocation.

    @return The number of entries in the most recent generation recorded
            in the log directory.
*/
unsigned long
ld_numWorkingEntries(void) {
  return generation_table[0].count;
}


/** Remove a log directory entry.

    This entry is designed for re-typing disk frames (currently between
    holding nodes and holding pages). It removes all information about
    both primary and previous primary locations from the directory.

    @param[in] oid Is the OID to remove.
*/
void
ld_removeObjectEntry(OID oid) {
  TreeHead *tree = &log_directory;
  TreeNode *n = find_node(tree, oid);
  if (NULL == n) return;
    
  tree_remove_node(tree, n);
}



/** Get the size of an individual log directory entry.

    Get the size of the individual storage elements used to record OIDs
    in the log. Used during initialization to have an external procedure
    allocate the space for the log directory.

    @return The size of a single entry in the log directory.
*/
uint32_t
ld_getDirEntrySize(void) {
  return sizeof(TreeNode);
}


/** Define the storage for the log directory.

    Define the storage to be used by the log directory. It must be large
    enough to contain numLogDirEntries of ld_getDirEntrySize each. The caller
    must fill in numLogDirEntries before calling this routine.

    @param[in] logDirectory Is the address of the area.
*/
void
ld_defineDirectory(void * logDirectory) {
  logDirNodes = (TreeNode *)logDirectory;
}


#ifdef SELF_TEST
/* Code to unit test the RB tree logic */

static uint64_t rand_state = 34567;
/** Random number generator. Not a truely wonderful one, but small.
 
    @return A 32 bit random number based on the value of rand_state.
*/
static uint32_t
rand(void) {
  rand_state = (rand_state * 69069 + 5) & 0xffffffffffffffffll;
  return (rand_state >> 32) & 0x7fffffff;
}

#ifdef NOT_USED
/** Compare two object descriptors for equality.

    @param[in] a The first object descriptor to compare.
    @param[in] b The second object descriptor to compare.
    @return TRUE if they are equal, FALSE otherwise.
*/
static bool
odEqual(const ObjectDescriptor *a, const ObjectDescriptor *b) {
  if (a->oid != b->oid) return false;
  if (a->allocCount != b->allocCount) return false;
  if (a->callCount != b->callCount) return false;
  if (a->logLoc != b->logLoc) return false;
  if (a->type != b->type) return false;
  return true;
}
#endif

/** Print the contents of an object descriptor.

    @param[in] a The object descriptor to print.
*/
static void
printOD(const ObjectDescriptor *a) {
  if (a != NULL) {
    printf("oid=%lld\n   allocCount=%d\n   callCount=%d\n"
	   "   logLoc=%lld\n   type=%d\n",
	   a->oid, a->allocCount, a->callCount, a->logLoc,
	   a->type);
  } else {
    printf("NULL\n");
  }
}


/** Make a generation worth of entries and put them in the tree
    with ld_recordLocation.

    @param[in] generation The generation number to create.
    @param[in] randomSeed The seed for generating random numbers.
    @return The number of entries created.
*/
#define MAX_TEST_OBJECTS 1000
static int
makeAGeneration(GenNum generation, uint64_t randomSeed) {
  int this_pass;
  uint32_t r;
  int i;

  ObjectDescriptor t = {
    55, /* oid */
    27, /* alloccount */
    43, /* call count */
    0, /* log loc */
    1};   /* type */
  
  rand_state = randomSeed;
  this_pass = rand() % MAX_TEST_OBJECTS;
  
  printf("Writing %d objects for generation %d\n", this_pass, generation);
    
  for (i=0; i<this_pass; i++) {
    r = rand();
    t.oid = r;
    t.logLoc = i+MAX_TEST_OBJECTS*generation;
    t.allocCount = r / 5; 
    t.callCount = r / 3;
    t.type = (r / 7 ) & 1;
    
    //        printf("t: "); printOD(&t);
    
    ld_recordLocation(&t, generation);
  }
  assert(i == ld_numWorkingEntries());
  return i;
}


/** Test a generation worth of entries in the tree.

    @param[in] generation The generation number to create.
    @param[in] randomSeed The seed for generating random numbers.
    @param[in] oid The oid of an object which has been deleted or zero
*/
static void
checkAGeneration(GenNum generation, uint64_t randomSeed, OID oid) {
  int this_pass;
  uint32_t r;
  int i;
  
  rand_state = randomSeed;
  this_pass = rand() % MAX_TEST_OBJECTS;

  printf("Running findNextObject for generation %d\n", generation);

  const ObjectDescriptor *od;
  int count = 0;
  ld_resetScan(generation);
  for (od=ld_findNextObject(generation);
       NULL!=od;
       od=ld_findNextObject(generation)) {
    count++;
  }
  if (count + (0==oid ? 0 : 1) != this_pass) {
    printf("Generation %d built with %d objects, scanned %d, deletedOid %lld\n",
	   generation, this_pass, count, oid);
    assert(FALSE);
  }

  for (i=0; i<this_pass; i++) {
    r = rand();
    const ObjectDescriptor *rv = ld_findObject(r);
    if (r == oid) {
      assert(NULL == rv);
    } else {
      // printf("r: "); printOD(rv);
      if (rv->oid != r || rv->callCount != (r/3) || rv->allocCount != (r/5)
	  || rv->type != ((r/7)&1)
	  || rv->logLoc != i+MAX_TEST_OBJECTS*generation) {
	printf("ld_findObject failed on OID %d\n", r);
	printf("rv: "); printOD(rv);
	printf("oid=%d, allocCount=%d, callCount=%d, type=%d, logLoc= %d\n",
	       r, r/5, r/3, (r/7)&1, i+MAX_TEST_OBJECTS*generation);
	assert(FALSE);
      }
    }
  }
  printf("Fetched %d objects for generation %d\n", count, generation);
}


/** Test a previous primary generation worth of entries in the tree.

    @param[in] generation The generation number of the previous entry.
    @param[in] randomSeed The seed for generating random numbers.
    @param[in] oid The oid of an object which has been deleted or zero
*/
static void
checkDuplicateGeneration(GenNum generation, uint64_t randomSeed, OID oid) {
  int this_pass;
  uint32_t r;
  int i;
  
  rand_state = randomSeed;
  this_pass = rand() % MAX_TEST_OBJECTS;

  printf("Fetching %d objects for previous generation %d\n",
	 this_pass, generation);

  for (i=0; i<this_pass; i++) {
    r = rand();
    const LID *pl = ld_findObjectForJournal(r, generation+1);
    const ObjectDescriptor *cv = ld_findObject(r);
    if (r == oid) {
      assert(NULL == pl);
      assert(NULL == cv);
    } else if (0==i && 0==oid) { /* Checking before primary deleted */
      assert(NULL == pl);
      assert(NULL != cv);
    } else {
      assert (*pl != cv->logLoc);
      // printf("r: "); printOD(rv);
    }
  }
}

typedef struct {
  uint64_t rand_state; /* Rand generator state at start of generation */
  OID deleted_oid;     /* OID of deleted entry or 0 */
  int count;           /* Number of directory nodes in this generation */
} test_state;

/** Run a unit test on the log directory.
 */
int main() {
#define NBR_GENERATIONS 100
#define MIGRATE_DEPTH 5  /* How many generations the migrator is behind */
#define RETIRED_DEPTH (MIGRATE_DEPTH+3) /* How many generations to keep */
#define INTRODUCE_DUPLICATES (MIGRATE_DEPTH-2)
#define GENERATION_OFFSET 1000
#define TEST_DIR_ENTRIES 6000
  test_state state[NBR_GENERATIONS] = { {0,0,0} };
  int i, j;
  TreeNode dirStorage[TEST_DIR_ENTRIES];

  numLogDirEntries = TEST_DIR_ENTRIES;
  ld_defineDirectory(dirStorage);

  printf("log_directory at 0x%08x\n", &log_directory);
  printf("TREE_NIL at 0x%08x\n", TREE_NIL);
  printf("numLogDirEntries=%d\n", numLogDirEntries);
  rand(); /* First random number is zero, clear it */

  for (i=0; i<NBR_GENERATIONS; i++) {
    int generation = i + GENERATION_OFFSET;
    uint64_t high_random_state = rand_state;
    
    if (0 == (i % INTRODUCE_DUPLICATES) && i>0 ) {
      printf("Starting duplicate generation %d\n", generation);
      state[i].rand_state = state[i-INTRODUCE_DUPLICATES].rand_state;
    } else {
      state[i].rand_state = rand_state;
      printf("Starting generation %d\n", generation);
    }

    /* Make a generation of entries and check them */
    state[i].count = makeAGeneration(generation, state[i].rand_state);
    checkAGeneration(generation, state[i].rand_state, 0);

    /* If we made entries to duplicate a previous generation, check
       the previous primary generation */
    if (0 == (i % INTRODUCE_DUPLICATES) && i>0 ) {
      printf("Checking previous entries for generation %d\n", generation);
      checkDuplicateGeneration(generation-INTRODUCE_DUPLICATES,
			       state[i].rand_state, 0);
      state[i-INTRODUCE_DUPLICATES].count = 0; /* No entries used by old gen */
    }

    /* Delete an entry in the most recent generation */
    {
      rand_state = state[i].rand_state;
      OID oid = rand(); /* Toss first random number */
      if (0 != oid) {
	oid = rand();     /* Get oid to delete */
	assert(NULL != ld_findObject(oid));
	ld_removeObjectEntry(oid);
	state[i].count--;
	state[i].deleted_oid = oid;
      } else {
	state[i].deleted_oid = 0;
      }
      assert(ld_numWorkingEntries() == state[i].count);
      checkAGeneration(generation, state[i].rand_state, state[i].deleted_oid);
      
      /* If we made entries to duplicate a previous generation, check
	 the previous primary generation */
      if (0 == (i % INTRODUCE_DUPLICATES) && i>0 ) {
	printf("Checking previous entries for generation %d\n", generation);
	checkDuplicateGeneration(generation-INTRODUCE_DUPLICATES,
				 state[i].rand_state, state[i].deleted_oid);
      }
    }

    /* Inform the log directory of retired generations */
    if (i>=MIGRATE_DEPTH) {
      printf("Marking generation %d as retired\n", generation-MIGRATE_DEPTH);
      ld_generationRetired(generation-RETIRED_DEPTH);
    }

    /* Clear out the oldest entrie(s) */
    if (i>=RETIRED_DEPTH) {
      printf("Clearing generation %d\n", generation-RETIRED_DEPTH);
      ld_clearGeneration(generation-RETIRED_DEPTH);
      state[i-RETIRED_DEPTH].count = 0;
    }
    for (j=i-INTRODUCE_DUPLICATES+1; j<=i; j++) {
      if (j<0) continue;
      checkAGeneration(j+GENERATION_OFFSET, state[j].rand_state,
		       state[j].deleted_oid);
    }
    if (0 == (i % INTRODUCE_DUPLICATES) && i>0 ) {
      rand_state = high_random_state;
    }

    /* Check the number of available entries */
    {
      int ent_count = numLogDirEntries;
      int ii;
      for (ii=0; ii<RETIRED_DEPTH; ii++) {
	int jj = i - ii;
	if (jj < 0) continue;
	ent_count -= state[jj].count;
      }
      int avail = ld_numAvailableEntries();
      assert(avail == ent_count);
    }
  }
  /* Clear all generations */
  for (i=NBR_GENERATIONS-INTRODUCE_DUPLICATES; i<NBR_GENERATIONS; i++) {
    printf("Clearing generation %d\n", i+GENERATION_OFFSET);
    ld_clearGeneration(i+GENERATION_OFFSET);
    for (j=i+1; j<NBR_GENERATIONS; j++) {
      checkAGeneration(j+GENERATION_OFFSET, state[j].rand_state,
		       state[j].deleted_oid);
    }
  }
  return 0;
}
#endif /* SELF_TEST */
