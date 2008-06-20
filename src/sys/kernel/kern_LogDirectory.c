/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
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

#include "kerninc/LogDirectory.h"

#define FALSE false
#define TRUE true

#define TREE_RED 0
#define TREE_BLACK 1

#define NUMBER_LOG_DIR_ENTRIES 5000

typedef struct TreeNode {
  	uint64_t generation;
	ObjectDescriptor od;
  /* Data for the RB tree */
	struct TreeNode *left;
  	struct TreeNode *right;
  	struct TreeNode *parent;
  	int color;
  /* Data for the doublely linked list */
        struct TreeNode *prev;
        struct TreeNode *next;
} TreeNode;

#define TREE_NIL (&log_directory_nil_node)
TreeNode log_directory_nil_node = {
  0, { 0, 0, 0, 0, 0, 0, 0}, TREE_NIL, TREE_NIL, TREE_NIL, TREE_BLACK,
  NULL, NULL};

TreeNode nodes[NUMBER_LOG_DIR_ENTRIES];
TreeNode *free_list = NULL;

typedef struct TreeHead {
  TreeNode *root;
} TreeHead;

/** Objects cleaned more recently than the restart generation.

    When searching for the most recent location of an object, this
    tree will be searched first. If no entry is found in this tree,
    the log_directory tree will be searched next.
 */
TreeHead working_directory = {TREE_NIL};

/** Objects from the restart generation and earlier generations.

    When looking for a log location for a journal write, this is the
    only tree that will be searched.
 */
TreeHead log_directory = {TREE_NIL};

/** Headers for lists of all TreeNodes of a given generation.

    In addition to two trees we keep for locating the working generation
    version of an OID and the most recent of the restart and previous
    generations. Each of nodes are kept in a linked list by generation
    to enable us to quickly delete them when we overlay an old generation
    with more recent data.

    Generations older than highest_generation - LD_MAX_GENERATIONS 
    are all lumped together in the list addressed by 
    generation_table[LD_MAX_GENERATIONS].
 */
#define LD_MAX_GENERATIONS 15
typedef struct {
  TreeNode *head;
  TreeNode *cursor;
} GT;

GT generation_table[LD_MAX_GENERATIONS+1];
uint64_t highest_generation = 0;
int log_entry_count = 0; /** Number of allocated TreeNodes */

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
#ifndef NDEBUG
  tn->left = tn->right = tn->parent = tn->prev = NULL;
  tn->generation = -1;
  if (tn == generation_table[0].head) assert(FALSE);
#endif
  tn->next = free_list;
  free_list = tn;
  assert( (--log_entry_count) >= 0);
}


/** Return the index in the generation table for a given generation.

    Note that all generations older than LD_MAX_GENERATIONS are kept in
    the last entry.

    @param[in] generation The generation for which the index is wanted
    @return The index for the generation table.
*/
static int
get_generation_index(uint64_t generation) {
  uint64_t gdelta = highest_generation - generation;
  return (gdelta > LD_MAX_GENERATIONS ? LD_MAX_GENERATIONS : gdelta);
}


/** Merge a list into the lumped together list.

    @param[in] tn A TreeNode in the list to merge.
*/
static void
merge_list(/*NullOK*/ TreeNode *tn) {
  if (NULL == tn) return; /* Nothing to do */
  assert(NULL == generation_table[LD_MAX_GENERATIONS].cursor);
  if (NULL ==generation_table[LD_MAX_GENERATIONS].head) {
    /* There are no entries in the lumped end. Just move these there. */
    generation_table[LD_MAX_GENERATIONS].head = tn;
  } else {
    TreeNode *x = generation_table[LD_MAX_GENERATIONS].head;
    x->prev->next = tn->prev;
    tn->prev->next = x->prev;
    x->prev = tn;
    tn->prev = x;
  }
}

/** Chain a node into the generation chain.

    @param[in] n The node to chain.
*/
static void
chain_node(TreeNode *n) {
  int gti = get_generation_index(n->generation);
  if (NULL == generation_table[gti].head) {
    generation_table[gti].head = n;
    n->next = n;
    n->prev = n;
  } else {
    TreeNode *t = generation_table[gti].head;
    n->next = t->next;
    t->next->prev = n;
    n->prev = t;
    t->next = n;
  }
}

#ifndef NDEBUG
/** Sanity check the generaion chain. */
static bool
chains_validate(void) {
  int count = 0;
  int gti;
  for (gti=0; gti<=LD_MAX_GENERATIONS; gti++) {
    if (NULL == generation_table[gti].head) {
      assert(NULL == generation_table[gti].cursor);
      continue;
    }
    TreeNode *cursor = generation_table[gti].cursor;
    TreeNode *last = generation_table[gti].head;
    TreeNode *x = last->next;
    TreeNode *y = x->next;
    bool cursor_found = (NULL == cursor);
    for (;;) {
      if (!cursor_found) cursor_found = (x == cursor);
      assert(y->prev == x);
      assert(x->next == y);
      count++;
      if (x == last) break;
      x = y;
      y = x->next;
    }
  }
  if (count == log_entry_count) return TRUE;
  printf("%d TreeNodes allocated, %d in chains\n", log_entry_count, count);
  return FALSE;
}
#endif


/** Remove a node from the generation chain.

    @param[in] n The node to chain.
*/
static void
unchain_node(TreeNode *n) {
  int gti = get_generation_index(n->generation);
  GT *gte = &generation_table[gti];
  if (gte->head == n) gte->head = n->prev;
  if (gte->head == n) gte->head = NULL;
  if (gte->cursor == n) gte->cursor = n->next;
  if (gte->cursor == n) gte->cursor = NULL;
  n->prev->next = n->next;
  n->next->prev = n->prev;
  n->next = n->prev = n;
}


/** Compare two object descriptors for tree operations.

    @param[in] od1 The first Object Descriptor to compare
    @param[in] od2 The second Object Descriptor to compare
    @return negative if od1 is less than od2, 0 if they are equal,
            and positive if od1 is greater than od2.
*/
static int
comp(const ObjectDescriptor *od1, const ObjectDescriptor *od2) {
  return od1->oid - od2->oid;
}

#ifndef NDEBUG

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
  if (!chains_validate()) return FALSE;

  return tree_validate_recurse(tree, node);
}
#endif /* NDEBUG */

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
    @param[in] tn The node to insert.
    @return A pointer to the node inserted, or NULL if an existing
    node was updated.
*/


static TreeNode *
binary_insert(TreeHead *tree, const ObjectDescriptor *od, 
	      uint64_t generation) {
#ifndef NDEBUG
  int whichcase;
#endif

  TreeNode *y = TREE_NIL;
  TreeNode *x = tree->root;

#ifndef NDEBUG
  assert( tree_validate(tree, tree->root) );
#endif
  
  while (x != TREE_NIL) {

    assert (x->left == TREE_NIL || x->left->parent == x);
    assert (x->right == TREE_NIL || x->right->parent == x);

    y = x;
    if (comp(od, &x->od) == 0) {
#ifdef VERBOSE
      printf("Update OID %lld in node 0x%08x\n", od->oid, x);
#endif
      unchain_node(x);
      x->generation = generation;
      chain_node(x);
      x->od = *od;
#ifndef NDEBUG
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

#ifndef NDEBUG
  assert( tree_validate(tree, tree->root) );
#endif
  TreeNode *tn = alloc_node();
  tn->od = *od;
  tn->left = TREE_NIL;
  tn->right = TREE_NIL;
  tn->parent = TREE_NIL;
  tn->generation = generation;
  chain_node(tn);

  tn->parent = y;

#ifdef VERBOSE
  printf("Insert OID %lld node 0x%08x into 0x%08x\n", tn->od.oid, tn, tree);
#endif
  
  if (y == TREE_NIL) {
#ifndef NDEBUG
    whichcase = 1;
#endif
    tree->root = tn;
  }
  else {
#ifndef NDEBUG
    whichcase = 2;
#endif
    
    if (comp(&tn->od, &y->od) < 0) { /*if ( CMP(tree, z, y) < 0 ) { */
      y->left = tn;
#ifndef NDEBUG
      assert( tree_validate(tree, tree->root) );
#endif
    }
    else {
      y->right = tn;
#ifndef NDEBUG
      assert( tree_validate(tree, tree->root) );
#endif
    }
    assert (y->left == TREE_NIL || y->left->parent == y);
    assert (y->right == TREE_NIL || y->right->parent == y);
  }

  assert(tree->root->parent == TREE_NIL);

#ifndef NDEBUG
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


#ifndef NDEBUG
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
#ifndef NDEBUG
  int whichcase = 0;
#endif
  
  TreeNode *y = TREE_NIL;
  TreeNode *x = TREE_NIL;
  
  assert(tree->root->parent == TREE_NIL);
  assert(TREE_NIL->color == TREE_BLACK);

#ifdef VERBOSE
  printf("Remove node 0x%08x from 0x%08x\n", z, tree);
#endif  

#ifndef NDEBUG
  if ( tree_validate(tree, tree->root) == FALSE ) {
    printf("Bad pre-remove validation, case 0x%08x\n", whichcase);
    assert (FALSE);
  }
#endif

  assert (z != TREE_NIL);

  if (z->left == TREE_NIL || z->right == TREE_NIL) {
    whichcase |= 0x1;
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
    whichcase |= 0x10;
    tree->root = x;
  }
  else {
    whichcase |= 0x20;
    if (y == y->parent->left) {
      y->parent->left = x;
    }
    else {
      y->parent->right = x;
    }
  }

#ifndef NDEBUG
  {
    if ( rbtree_contains(tree, y) ) {
      printf("x=0x%08x, y=0x%08x, z=0x%08x tree=0x%08x\n", x,
	      y, z, tree);
      printf("Deleted node 0x%08x still referenced before fixup!\n", y);
    }
  }
#endif

  /* Until I actually understand the fixup transformation, do this */
  /* just as the book would have us do and then fix the fact that the */
  /* wrong thing was deleted after the fact. */
  
  {
    ObjectDescriptor zvalue = z->od;	/* SHAP */
    uint64_t generation = z->generation;

    if (y != z) {
      unchain_node(z);  /* Remove z from the generation chain */
      z->od = y->od;
      z->generation = y->generation;
      chain_node(z);
    }
    if ( y->color == TREE_BLACK )
      remove_fixup(tree, x);

#ifndef NDEBUG
    if ( rbtree_contains(tree, (y!=z) ? y : z) )
      printf("Deleted znode 0x%08x still referenced by after fixup!\n", z);

    if ( tree_validate(tree, tree->root) == FALSE ) {
      printf("Bad post-remove validation, case 0x%08x\n",  whichcase);
      assert (FALSE);
    }
#endif

    if (y != z) {
#ifndef NDEBUG
      if ( rbtree_contains(tree, y) )
	printf("Deleted ynode (post fixup) 0x%08x still referenced "
		"by after fixup!\n", z);
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
#ifndef NDEBUG
  if ( rbtree_contains(tree, z) )
    printf("Deleted znode (post fixup) 0x%08x still "
	    "referenced by after fixup!\n", z);

  if ( tree_validate(tree, tree->root) == FALSE ) {
    printf("Bad post-remove validation, case 0x%08x\n", whichcase);
    assert (FALSE);
  }
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
void ld_recordLocation(const ObjectDescriptor *od, uint64_t generation) {
  assert(TREE_NIL->color == TREE_BLACK);
  TreeHead *tree;

  if (generation > highest_generation) {
    if (0 == highest_generation) {
      /* Startup. Initialize the generation table */
      int i;
      for (i=0; i<=LD_MAX_GENERATIONS; i++) {
	generation_table[i].head = NULL;
	generation_table[i].cursor = NULL;
      }
      /* Chain the TreeNodes on the free list */
      log_entry_count = NUMBER_LOG_DIR_ENTRIES;
      for (i=0; i<NUMBER_LOG_DIR_ENTRIES; i++) {
	free_node(&nodes[i]);
      }
      assert(0 == log_entry_count);
    } else {
      /* Move the most recent generation from the working directory
	 to the log directory. */
      TreeNode *tn = generation_table[0].head;
      if (NULL != tn) {
	TreeNode *end = tn->prev;
	for (;;) {
	  /* Save information from node to be deleted */
	  ObjectDescriptor od = tn->od;
	  uint64_t gen = tn->generation;
	  /* We are done with tn - keep pointer for loop end test */
	  TreeNode *next = tn->next;
	  tree_remove_node(&working_directory, tn);
	  TreeNode *nn = binary_insert(&log_directory, &od, gen);

	  if (NULL != nn) {
	    
	    nn->color = TREE_RED;
	    
	    tree_insert_fixup(&log_directory, nn);
	    
	    log_directory.root->color = TREE_BLACK;
	    assert(log_directory.root->parent == TREE_NIL);
	    assert(TREE_NIL->color == TREE_BLACK);
	    
#ifndef NDEBUG
	    if ( !tree_validate(&log_directory, log_directory.root) ) {
	      printf("Tree bad after ld_recordLocation()\n");
	      assert(FALSE);
	    }
#endif
	  }
	  if (tn == end) break;
	  tn = next;
	}
      }
      assert(working_directory.root == TREE_NIL);

      /* Move the generation table to accomodate the new generation */
      uint64_t move_size = generation - highest_generation;
      int ms = (move_size > LD_MAX_GENERATIONS
		? LD_MAX_GENERATIONS : move_size);
      int i;
      for (i=LD_MAX_GENERATIONS-1; i>=LD_MAX_GENERATIONS-ms; i--) {
	/* These generations get added to the lump at the end */
	merge_list(generation_table[i].head);
      }
      for (; i>=0; i--) {
	/* These generations are kept lower in the table */
	generation_table[i+ms] = generation_table[i];
      }
      for (i=ms-1; i>=0; i--) {
	/* And set the earliest entries to NULL */
	generation_table[i].head = NULL;
	generation_table[i].cursor = NULL;
      }
    }
    highest_generation = generation;
    tree = &working_directory;
  } else if (generation == highest_generation) {
    tree = &working_directory;
  } else {
    tree = &log_directory;
  }
  TreeNode *tn = binary_insert(tree, od, generation);

  if (NULL == tn) return;

  tn->color = TREE_RED;

  tree_insert_fixup(tree, tn);

  tree->root->color = TREE_BLACK;
  assert(tree->root->parent == TREE_NIL);
  assert(TREE_NIL->color == TREE_BLACK);

#ifndef NDEBUG
  if ( !tree_validate(tree, tree->root) ) {
    printf("Tree bad after ld_recordLocation()\n");
    assert(FALSE);
  }
#endif
}


/** Find an object in the directory.

    This routine will return the LIDs of objects in the working generation.

    @param[in] oid The object ID to be located.
    @return A pointer to the ObjectDescriptor for the object or NULL if the
            object is not in the log.
*/
const ObjectDescriptor *ld_findObject(OID oid) {
  TreeNode *n = find_node(&working_directory, oid);
  if (NULL != n) return &n->od;
  n = find_node(&log_directory, oid);
  if (NULL != n) return &n->od;
  return NULL;
}


/** Find the first object of a generation.

    This routine starts the scan of all objects in a generation.
    ld_nextObject continues the scan. There may be up to two scans in
    progress at any time (one for the checkpoint routines and one for
    migration). They are separated by using different generation numbers.

    Note the the order of objects in a generation is undefined. If it needs
    to be defined for some reason, like optimizing migration, then that
    need will be an additional constraint on the implementation of the
    object directory.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the first object in a generation scan.
*/
const ObjectDescriptor *ld_findFirstObject(uint64_t generation) {
  int gti = get_generation_index(generation);
  GT *gte = &generation_table[gti];
  if (NULL == gte->head) return NULL;
  gte->cursor = gte->head;
  return &gte->cursor->od;
}

/** Find the next object of a generation.

    This routine continues the scan of all objects in a generation.
    See ld_findFirstObject for more information.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the next object in a generation scan.
*/
const ObjectDescriptor *ld_findNextObject(uint64_t generation) {
  int gti = get_generation_index(generation);
  GT *gte = &generation_table[gti];
  if (NULL == gte->cursor) return NULL;
  if (gte->cursor->next == gte->head) {
    gte->cursor = NULL;
    return NULL;
  }
  gte->cursor = gte->cursor->next;
  return &gte->cursor->od;
}

/** Remove all the objects in a generation from the Object Directory.

    Note: This routine may need to be executed in smaller pieces to meet
    real-time requirements.

    @param uint64_t generation The generation to clear.
*/
void ld_clearGeneration(uint64_t generation) {
  assert (generation <= highest_generation);

  int gti = get_generation_index(generation);
  TreeNode *tn = generation_table[gti].head;
  TreeHead *tree = (0 == gti ? &working_directory : &log_directory);

  if (NULL == tn) return;
  while (tn->next != tn) {
    tree_remove_node(tree, tn->next);
  }
  tree_remove_node(tree, tn);
  generation_table[gti].head = NULL;
  generation_table[gti].cursor = NULL;
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
odEqual(ObjectDescriptor *a, ObjectDescriptor *b) {
  if (a->oid != b->oid) return false;
  if (a->allocCount != b->allocCount) return false;
  if (a->callCount != b->callCount) return false;
  if (a->logLoc != b->logLoc) return false;
  if (a->allocCountUsed != b->allocCountUsed) return false;
  if (a->callCountUsed != b->callCountUsed) return false;
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
	   "   logLoc=%lld\n   allocCountUsed=%d\n"
	   "   callCountUsed=%d\n   type=%d\n",
	   a->oid, a->allocCount, a->callCount, a->logLoc,
	   a->allocCountUsed, a->callCountUsed, a->type);
  } else {
    printf("NULL\n");
  }
}


/** Make a generation worth of entries and put them in the tree
    with ld_recordLocation.

    @param[in] generation The generation number to create.
    @param[in] randomSeed The seed for generating random numbers.
*/
#define MAX_TEST_OBJECTS 1000
static void
makeAGeneration(uint64_t generation, uint64_t randomSeed) {
  int this_pass;
  uint32_t r;
  int i;

  ObjectDescriptor t = {
    55, /* oid */
    27, /* alloccount */
    43, /* call count */
    0, /* log loc */
    true, /* alloc count used */
    true, /* call count used */
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
}


/** Test a generation worth of entries in the tree.

    @param[in] generation The generation number to create.
    @param[in] randomSeed The seed for generating random numbers.
*/
static void
checkAGeneration(uint64_t generation, uint64_t randomSeed) {
  int this_pass;
  uint32_t r;
  int i;
  
  rand_state = randomSeed;
  this_pass = rand() % MAX_TEST_OBJECTS;

  printf("Running findNextObject for generation %d\n", generation);

  const ObjectDescriptor *od;
  int count = 0;
  for (od=ld_findFirstObject(generation);
       NULL!=od;
       od=ld_findNextObject(generation)) {
    count++;
  }
  if (count != this_pass) {
    printf("Generation %d built with %d objects, scanned %d\n",
	   generation, this_pass, count);
    assert(FALSE);
  }
  printf("Fetching %d objects for generation %d\n", this_pass, generation);

  for (i=0; i<this_pass; i++) {
    r = rand();
    const ObjectDescriptor *rv = ld_findObject(r);
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


/** Run a unit test on the log directory.
 */
int main() {
#define NBR_GENERATIONS 100
#define MIGRATE_DEPTH 5
#define INTRODUCE_DUPLICATES (MIGRATE_DEPTH-2)
#define GENERATION_OFFSET 1000
  uint64_t save_random_state[NBR_GENERATIONS];
  int i, j;

  printf("working_directory at 0x%08x\n", &working_directory);
  printf("log_directory at 0x%08x\n", &log_directory);
  printf("TREE_NIL at 0x%08x\n", TREE_NIL);
  rand(); /* First random number is zero, clear it */

  for (i=0; i<NBR_GENERATIONS; i++) {
    int generation = i + GENERATION_OFFSET;
    uint64_t high_random_state = rand_state;
    
    if (0 == (i % INTRODUCE_DUPLICATES) && i>0 ) {
      printf("Starting duplicate generation %d\n", generation);
      save_random_state[i] = save_random_state[i-INTRODUCE_DUPLICATES];
    } else {
      save_random_state[i] = rand_state;
      printf("Starting generation %d\n", generation);
    }
    makeAGeneration(generation, save_random_state[i]);
    checkAGeneration(generation, save_random_state[i]);
    if (i>=MIGRATE_DEPTH) {
      printf("Clearing generation %d\n", generation-MIGRATE_DEPTH);
      ld_clearGeneration(generation-MIGRATE_DEPTH);
    }
    for (j=i-INTRODUCE_DUPLICATES+1; j<=i; j++) {
      if (j<0) continue;
      checkAGeneration(j+GENERATION_OFFSET, save_random_state[j]);
    }
    if (0 == (i % INTRODUCE_DUPLICATES) && i>0 ) {
      rand_state = high_random_state;
    }
  }
  /* Clear all but the last generation, can't clear the working generation */
  for (i=NBR_GENERATIONS-INTRODUCE_DUPLICATES; i<NBR_GENERATIONS; i++) {
    printf("Clearing generation %d\n", i+GENERATION_OFFSET);
    ld_clearGeneration(i+GENERATION_OFFSET);
    for (j=i+1; j<NBR_GENERATIONS; j++) {
      checkAGeneration(j+GENERATION_OFFSET, save_random_state[j]);
    }
  }
  return 0;
}
#endif /* SELF_TEST */
