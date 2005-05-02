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


#ifdef RB_TREE
TREENODE *
rb_tree_rightrotate(TREENODE *root, TREENODE *y);
TREENODE *
rb_tree_leftrotate(TREENODE *root, TREENODE *x);


TREENODE *
rb_tree_rightrotate(TREENODE *root, TREENODE *y)
{
  TREENODE *x = y->left;

  y->left = x->right;
  if (x->right != TREE_NIL)
    x->right->parent = y;

  x->parent = y->parent;
  if (x->parent == TREE_NIL)
    root = x;
  else if (y == y->parent->left)
    y->parent->left = x;
  else
    y->parent->right = x;

  x->right = y;
  y->parent = x;

  assert(root->parent == TREE_NIL);
  return root;
}

TREENODE *
rb_tree_leftrotate(TREENODE *root, TREENODE *x)
{
  TREENODE *y = x->right;

  x->right = y->left;
  if (y->left != TREE_NIL)
    y->left->parent = x;

  y->parent = x->parent;
  if (x->parent == TREE_NIL)
    root = y;
  else if (x == x->parent->left)
    x->parent->left = y;
  else
    x->parent->right = y;

  y->left = x;
  x->parent = y;

  assert(root->parent == TREE_NIL);
  return root;
}
#endif
