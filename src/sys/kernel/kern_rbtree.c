/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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

/* RB-Tree implementation from OpenCM code base */

#include <kerninc/kernel.h>
#include <kerninc/rbtree.h>

/*#define TREE_DEBUG*/

#ifdef TREE_DEBUG
#define tree_debug opt_RBTreeDebug

#ifndef tree_debug
tree_debug = 1;
#endif
#endif /* TREE_DEBUG */

#define CMPKEY(t, n, k) t->cmpkey(n, k)
#define CMP(t, n1, n2) rbtree_compare_nodes(t, n1, n2)

rbnode tree_nil_node = { TREE_NIL, TREE_NIL, TREE_NIL, 0, TREE_BLACK };

int 
rbtree_compare_node_to_key(rbtree *t, rbnode *n, rbkey*k)
{
  return CMPKEY(t, n, k);
}

int
rbtree_compare_nodes(rbtree *t, rbnode *n1, rbnode *n2)
{
  int result = t->cmp(n1, n2);

  if (result == 0 && t->multiKey) {
    if (n1->seqNo < n2->seqNo)
      return -1;
    else if (n1->seqNo > n2->seqNo)
      return 1;
  }

  return result;
}

#if 0
int 
rbtree_s_cmp(const rbnode *rn1, const rbnode *rn2)
{
  if (rn1->value.w < rn2->value.w)
    return -1;
  else if (rn1->value.w > rn2->value.w)
    return 1;
  else
    return strcmp(rn1->value.vp , rn2->value.vp);
}

int 
rbtree_s_cmpkey(const rbnode *rn1, const rbkey *rkey)
{
  if (rn1->value.w < rkey->w)
    return -1;
  else if (rn1->value.w > rkey->w)
    return 1;
  else
    return strcmp(rn1->value.vp , rkey->vp);
}
#endif


rbnode *
rbnode_create(const void *kvp, unsigned long kw, const void *data)
{
  rbnode *rbn = MALLOC(rbnode, sizeof(rbnode));
  rbn->left = TREE_NIL;
  rbn->right = TREE_NIL;
  rbn->parent = TREE_NIL;
  rbn->value.vp = kvp;
  rbn->value.w = kw;
  rbn->data = data;

  return rbn;
}

rbtree *
rbtree_create(int (*cmp)(const rbnode *, const rbnode *), 
	      int (*cmpkey)(const rbnode *, const rbkey *),
	      bool multiKey)
{
  rbtree *rbt = MALLOC(rbtree, sizeof(rbtree));

  rbt->seqNo = 0;
  rbt->multiKey = multiKey;
  rbt->root = TREE_NIL;
  rbt->cmp = cmp;
  rbt->cmpkey = cmpkey;

  return rbt;
}


static bool
rbtree_do_contains(rbnode *tree, rbnode *node)
{
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

bool
rbtree_contains(rbtree *tree, rbnode *node)
{
  return rbtree_do_contains(tree->root, node);
}

bool
rbtree_isEmpty(rbtree *tree)
{
  return (tree->root == TREE_NIL);
}

static rbnode *
rbtree_do_max(rbnode *tree)
{
  if (tree == TREE_NIL)
    return TREE_NIL;
  
  while (tree->right != TREE_NIL)
    tree = tree->right;

  return tree;
}

rbnode *
rbtree_max(rbtree *x)
{
  return rbtree_do_max(x->root);
}

static rbnode *
rbtree_do_min(rbnode *tree)
{
  if (tree == TREE_NIL)
    return TREE_NIL;
  
  while (tree->left != TREE_NIL)
    tree = tree->left;

  return tree;
}


rbnode *
rbtree_min(rbtree *x)
{
  return rbtree_do_min(x->root);
}

rbnode *
rbtree_pred(rbnode *x)
{
  rbnode *y;
  if (x == TREE_NIL)
    return TREE_NIL;
  
  if (x->left != TREE_NIL)
    return rbtree_do_max(x->left);

  y = x->parent;

  while (y != TREE_NIL && x == y->left) {
    x = y;
    y = y->parent;
  }

  return y;
}

rbnode *
rbtree_succ(rbnode *x)
{
  rbnode *y;

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

#define TREE_DEBUG
#define TRUE true
#define FALSE false
#ifdef TREE_DEBUG
bool
rbtree_validate(rbtree *tree, rbnode *node)
{
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
    printf("tree 0x%08x parent 0x%08x not nil!\n", tree, node->parent);
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

  if (node->left != TREE_NIL && tree->cmp(node->left,node) >= 0) {
    printf("left child's value. node 0x%08x nd->l 0x%08x!\n",
	    node, node->left);
    return FALSE;
  }
  
  if (node->right != TREE_NIL && node->right->parent != node) {
    printf("right child's parent!\n");
    return FALSE;
  }

  if (node->right != TREE_NIL && tree->cmp(node->right, node) <= 0) {
    printf("right child's value. node 0x%08x nd->r 0x%08x!\n",
	    node, node->right);
    return FALSE;
  }

  if ( rbtree_validate(tree, node->left) == FALSE )
    return FALSE;
  
  return rbtree_validate(tree, node->right);
}
#endif /* TREE_DEBUG */
#undef TREE_DEBUG

rbnode *
rbtree_find(rbtree *tree, rbkey *key)
{
  rbnode *n = tree->root;
  
  while (n != TREE_NIL) {
    int result = CMPKEY(tree, n,key);
    if (result == 0) {

      for ( ;tree->multiKey; ) {
	rbnode *pred = rbtree_pred(n);

	if (pred == TREE_NIL)
	  break;

	if (CMPKEY(tree, pred, key) == 0)
	  n = pred;
	else
	  break;
      }

      return n;
    }
    else if (result > 0)
      n = n->left;
    else
      n = n->right;
  }

  return TREE_NIL;
}

static void
binary_insert(rbtree *tree, rbnode *z)
{
#ifdef TREE_DEBUG
  int whichcase;
#endif

  rbnode *y = TREE_NIL;
  rbnode *x = tree->root;

#ifdef VERBOSE
  xprintf("Insert node 0x%08x into 0x%08x\n", z, root);
#endif
  
  z->parent = TREE_NIL;
  z->left = TREE_NIL;
  z->right = TREE_NIL;
  
#ifdef TREE_DEBUG
  if (tree_debug)
    assert( rbtree_validate(tree, tree->root) );
#endif
  
  while (x != TREE_NIL) {

    assert (x->left == TREE_NIL || x->left->parent == x);
    assert (x->right == TREE_NIL || x->right->parent == x);

    y = x;
    if ( CMP(tree,z,x) < 0 ) {
      x = x->left;
    }
    else {
      x = x->right;
    }
  }

#ifdef TREE_DEBUG
  if (tree_debug)
    assert( rbtree_validate(tree, tree->root) );
#endif

  z->parent = y;

  if (y == TREE_NIL) {
#ifdef TREE_DEBUG
    whichcase = 1;
#endif
    tree->root = z;
  }
  else {
#ifdef TREE_DEBUG
    whichcase = 2;
#endif
    
    if ( CMP(tree, z, y) < 0 ) {
      y->left = z;
#ifdef TREE_DEBUG
      if (tree_debug)
	assert( rbtree_validate(tree, tree->root) );
#endif
    }
    else {
      y->right = z;
#ifdef TREE_DEBUG
      if (tree_debug)
	assert( rbtree_validate(tree, tree->root) );
#endif
    }


    assert (y->left == TREE_NIL || y->left->parent == y);
    assert (y->right == TREE_NIL || y->right->parent == y);
  }

  assert(tree->root->parent == TREE_NIL);

#ifdef TREE_DEBUG
  if ( tree_debug && rbtree_validate(tree, tree->root) == FALSE ) {
    xprintf("Bad post-insert validation, case %d\n", whichcase);
    assert (FALSE);
  }
#endif
}

static void
rb_tree_rightrotate(rbtree *tree, rbnode *y)
{
  rbnode *x = y->left;

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

static void
rb_tree_leftrotate(rbtree *tree, rbnode *x)
{
  rbnode *y = x->right;

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

static void
rbtree_insert_fixup(rbtree *tree, rbnode *x)
{
  assert(TREE_NIL->color == TREE_BLACK);

  while (x != tree->root && x->parent->color == TREE_RED) {
    if (x->parent == x->parent->parent->left) {
      rbnode *y = x->parent->parent->right;
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
      rbnode *y = x->parent->parent->left;
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

void
rbtree_insert(rbtree *tree, rbnode *x)
{
  assert (x != TREE_NIL);
  x->seqNo = tree->seqNo++;

  assert(TREE_NIL->color == TREE_BLACK);

  binary_insert(tree, x);

  x->color = TREE_RED;

  rbtree_insert_fixup(tree, x);

  tree->root->color = TREE_BLACK;
  assert(tree->root->parent == TREE_NIL);
  assert(TREE_NIL->color == TREE_BLACK);

#ifdef TREE_DEBUG
  if ( tree_debug && !rbtree_validate(tree, tree->root) ) {
    xprintf("Tree bad after RB_Insert()\n");
    assert(FALSE);
  }
#endif
}

/* Following declaration prevents warnings in -Wall mode: */
static void
rb_remove_fixup(rbtree *tree, rbnode *x)
{
  assert(TREE_NIL->color == TREE_BLACK);

  if (x->parent == TREE_NIL)	/* deleted last node in tree */
    return;
  
  while ((x != tree->root) && (x->color == TREE_BLACK)) {
    /* MacManis checks x == nilnode && x.parent.left == null OR */
    if (x == x->parent->left) {
      rbnode *w = x->parent->right;
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
      rbnode *w = x->parent->left;

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

void
rbtree_remove(rbtree * tree, rbnode *z)
{
  /*#ifdef TREE_DEBUG*/
  int whichcase = 0;
  /*#endif*/
  
  rbnode *y = TREE_NIL;
  rbnode *x = TREE_NIL;
  
  assert(tree->root->parent == TREE_NIL);
  assert(TREE_NIL->color == TREE_BLACK);

#ifdef VERBOSE
  VERB_PRINTF((VERB_FIL, "Remove node 0x%08x from 0x%08x\n", z, root));
#endif  

#ifdef TREE_DEBUG
#if 0
  if (tree_debug)
    assert ( rbtree_validate(tree, tree->root) );
#else
  if ( tree_debug && rbtree_validate(tree, tree->root) == FALSE ) {
    xprintf("Bad pre-remove validation, case 0x%08x\n", whichcase);
    assert (FALSE);
  }
#endif
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

#ifdef TREE_DEBUG
  {
    if ( tree_debug && rbtree_contains(tree, y) ) {
      xprintf("x=0x%08x, y=0x%08x, z=0x%08x root=0x%08x\n", x,
	      y, z, tree->root);
      xprintf("Deleted node 0x%08x still referenced before fixup!\n", y);
    }
  }
#endif

  /* Until I actually understand the fixup transformation, do this */
  /* just as the book would have us do and then fix the fact that the */
  /* wrong thing was deleted after the fact. */
  
  {
    rbkey zvalue = z->value;	/* SHAP */

    if (y != z)
      z->value = y->value;
  
    if ( y->color == TREE_BLACK )
      rb_remove_fixup(tree, x);

#ifdef TREE_DEBUG
    if ( tree_debug && rbtree_contains(tree, (y!=z) ? y : z) )
      xprintf("Deleted znode 0x%08x still referenced by after fixup!\n", z);

    if ( tree_debug && rbtree_validate(tree, tree->root) == FALSE ) {
      xprintf("Bad post-remove validation, case 0x%08x\n",  whichcase);
      assert (FALSE);
    }
#endif

    if (y != z) {
#ifdef TREE_DEBUG
      if ( tree_debug && rbtree_contains(tree, y) )
	xprintf("Deleted ynode (post fixup) 0x%08x still referenced "
		"by after fixup!\n", z);
#endif

      /* The tree is now correct, but for the slight detail that we have
       * deleted the wrong node.  It needed to be done that way for
       * rb_remove_fixup to work.  At this point, Y is unreferenced, and
       * Z (the node we intended to delete) is firmly wired into the
       * tree.  Put Y in in Z's place and restore the old value to Z:
       *
       * At this point, Y and Z both have the correct bodies.  We need
       * to restore the correct key to Z, and swap the pointers
       * around so that Y gets inserted in place of Z.
       *
       * restore Z's value, which we smashed above.  */
      z->value = zvalue;

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
#ifdef TREE_DEBUG
  if ( tree_debug && rbtree_contains(tree, z) )
    xprintf("Deleted znode (post fixup) 0x%08x still "
	    "referenced by after fixup!\n", z);

  if ( tree_debug && rbtree_validate(tree, tree->root) == FALSE ) {
    xprintf("Bad post-remove validation, case 0x%08x\n", whichcase);
    assert (FALSE);
  }
#endif

  assert (TREE_NIL->color == TREE_BLACK);
}
