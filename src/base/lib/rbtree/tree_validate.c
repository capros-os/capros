/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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


#ifndef NDEBUG
bool
tree_validate(TREENODE *root, TREENODE *node)
{
  if (node == TREE_NIL)
    return true;
  
  if (node != 0) {
    if (node->parent == 0) {
      ERROR_PRINTF((ERR_FIL, "node 0x%08x parent is zero!\n",
		    node));
      return false;
    }
    if (node->left == 0) {
      ERROR_PRINTF((ERR_FIL, "node 0x%08x parent is zero!\n",
		    node));
      return false;
    }
    if (node->right == 0) {
      ERROR_PRINTF((ERR_FIL, "node 0x%08x parent is zero!\n",
		    node));
      return false;
    }
  }

  if (node == root && node->parent != TREE_NIL) {
    ERROR_PRINTF((ERR_FIL, "root 0x%08x parent 0x%08x not nil!\n",
		  root, node->parent));
    return false;
  }
  
  if (node != root && node->parent == TREE_NIL) {
    ERROR_PRINTF((ERR_FIL, "non-root parent nil!\n"));
    return false;
  }

  if (node->left != TREE_NIL && node->left->parent != node) {
    ERROR_PRINTF((ERR_FIL, "left child's parent!\n"));
    return false;
  }

  if (node->left != TREE_NIL && tree_compare(node->left,node) >= 0) {
    ERROR_PRINTF((ERR_FIL, "left child's value. node 0x%08x nd->l 0x%08x!\n",
	    node, node->left));
    return false;
  }
  
  if (node->right != TREE_NIL && node->right->parent != node) {
    ERROR_PRINTF((ERR_FIL, "right child's parent!\n"));
    return false;
  }

  if (node->right != TREE_NIL && tree_compare(node->right, node) <= 0) {
    ERROR_PRINTF((ERR_FIL, "right child's value. node 0x%08x nd->r 0x%08x!\n",
	    node, node->right));
    return false;
  }

  if ( tree_validate(root, node->left) == false )
    return false;
  
  return tree_validate(root, node->right);
}
#endif /*! NDEBUG*/

