/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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


/* Returns new root */
TREENODE *
binary_insert(TREENODE *root, TREENODE *z);

TREENODE *
binary_insert(TREENODE *root, TREENODE *z)
{
  int whichcase;

  TREENODE *y = TREE_NIL;
  TREENODE *x = root;

#ifdef VERBOSE
  VERB_PRINTF((VERB_FIL, "Insert node 0x%08x into 0x%08x\n", z, root));
#endif
  
  z->parent = TREE_NIL;
  z->left = TREE_NIL;
  z->right = TREE_NIL;
  
  assert( tree_validate(root, root) );
  
  while (x != TREE_NIL) {
    assert (x->left == TREE_NIL || x->left->parent == x);
    assert (x->right == TREE_NIL || x->right->parent == x);

    y = x;
    if ( tree_compare(z,x) < 0 ) {
      x = x->left;
    }
    else {
      x = x->right;
    }
  }

  z->parent = y;
  if (y == TREE_NIL) {
    whichcase = 1;
    root = z;
  }
  else {
    whichcase = 2;
    
    if ( tree_compare(z, y) < 0 )
      y->left = z;
    else
      y->right = z;

    assert (y->left == TREE_NIL || y->left->parent == y);
    assert (y->right == TREE_NIL || y->right->parent == y);
  }

  assert(root->parent == TREE_NIL);

#ifndef NDEBUG
  if ( tree_validate(root, root) == false ) {
    ERROR_PRINTF((ERR_FIL, "Bad post-insert validation, case %d\n",
	    whichcase));
    assert (false);
  }
#endif

  return root;
}


#ifdef RB_TREE
TREENODE *
tree_insert(TREENODE * root, TREENODE *x)
{
  root = binary_insert(root, x);

  x->color = TREE_RED;

  while (x != root && x->parent->color == TREE_RED) {
    assert (x->parent->parent);
    
    if (x->parent == x->parent->parent->left) {
      TREENODE *y = x->parent->parent->right;
      if (y->color == TREE_RED) {
	x->parent->color = TREE_BLACK;
	y->color = TREE_BLACK;
	x->parent->parent->color = TREE_RED;
	x = x->parent->parent;
      }
      else {
	if (x == x->parent->right) {
	  x = x->parent;
	  root = rb_tree_leftrotate(root, x);
	}
	x->parent->color = TREE_BLACK;
	assert (x->parent->parent);
	x->parent->parent->color = TREE_RED;
	root = rb_tree_rightrotate(root, x->parent->parent);
      }
    }
    else {
      TREENODE *y = x->parent->parent->left;
      if (y->color == TREE_RED) {
	x->parent->color = TREE_BLACK;
	y->color = TREE_BLACK;
	assert (x->parent->parent != TREE_NIL);
	x->parent->parent->color = TREE_RED;
	x = x->parent->parent;
      }
      else {
	if (x == x->parent->left) {
	  x = x->parent;
	  root = rb_tree_rightrotate(root, x);
	}
	x->parent->color = TREE_BLACK;
	assert (x->parent->parent != TREE_NIL);
	x->parent->parent->color = TREE_RED;
	root = rb_tree_leftrotate(root, x->parent->parent);
      }
    }
  }

  root->color = TREE_BLACK;
  assert(root->parent == TREE_NIL);

#ifndef NDEBUG
  if ( !tree_validate(root, root) ) {
    ERROR_PRINTF((ERR_FIL, "Tree bad after RB_Insert()\n"));
    assert(false);
  }
#endif

  return root;
}
#else
TREENODE *
tree_insert(TREENODE *root, TREENODE *z)
{
  return binary_insert(root, z);
}
#endif
