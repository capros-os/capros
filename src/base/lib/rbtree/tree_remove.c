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
/* Following declaration prevents warnings in -Wall mode: */
TREENODE *
rb_remove_fixup(TREENODE * root, TREENODE *x);

TREENODE *
rb_remove_fixup(TREENODE * root, TREENODE *x)
{
  TREENODE *w = TREE_NIL;

  if (x->parent == TREE_NIL)	/* deleted last node in tree */
    return root;
  
#ifdef BROKEN
  assert (x != TREE_NIL);
#endif
  
  while ((x != root) && (x->color == TREE_BLACK)) {

    /* MacManis checks x == nilnode && x.parent.left == null OR */
    if (x == x->parent->left) {
      w = x->parent->right;

      if (w->color == TREE_RED) {
	w->color = TREE_BLACK;
	assert (x->parent != TREE_NIL);
	x->parent->color = TREE_RED;
	root = rb_tree_leftrotate(root, x->parent);
	w = x->parent->right;
      }

      if ((w->left->color == TREE_BLACK) && (w->right->color == TREE_BLACK)) {
	assert (w != TREE_NIL);
	w->color = TREE_RED;
	x = x->parent;		/* move up the tree */
      }
      else {
	if (w->right->color == TREE_BLACK) {
	  w->left->color = TREE_BLACK;
	  assert (w != TREE_NIL);
	  w->color = TREE_RED;
	  root = rb_tree_rightrotate(root, w);
	  w = x->parent->right;
	}

	w->color = x->parent->color;
	x->parent->color = TREE_BLACK;
	w->right->color = TREE_BLACK;
	root = rb_tree_leftrotate(root, x->parent);
	x = root;
      }
    }
    else {
      w = x->parent->left;

      if (w->color == TREE_RED) {
	w->color = TREE_BLACK;
	assert (x->parent != TREE_NIL);
	x->parent->color = TREE_RED;
	root = rb_tree_rightrotate(root, x->parent);
	w = x->parent->left;
      }

      if ((w->right->color == TREE_BLACK) && (w->left->color == TREE_BLACK)) {
	assert (w != TREE_NIL);
	w->color = TREE_RED;
	x = x->parent;
      }
      else {
	if (w->left->color == TREE_BLACK) {
	  w->right->color = TREE_BLACK;
	  assert (w != TREE_NIL);
	  w->color = TREE_RED;
	  root = rb_tree_leftrotate(root, w);
	  w = x->parent->left;
	}
	w->color = x->parent->color;
	x->parent->color = TREE_BLACK;
	w->left->color = TREE_BLACK;
	root = rb_tree_rightrotate(root, x->parent);
	x = root;
      }
    }
  }
  
  x->color = TREE_BLACK;
  assert(root->parent == TREE_NIL);

  return root;
}

TREENODE *
tree_remove(TREENODE * root, TREENODE *z)
{
  int whichcase = 0;
  
  TREENODE *y = TREE_NIL;
  TREENODE *x = TREE_NIL;
  
#ifdef VERBOSE
  VERB_PRINTF((VERB_FIL, "Remove node 0x%08x from 0x%08x\n", z, root));
#endif  

#ifndef NDEBUG
#if 0
  assert ( tree_validate(root, root) );
#else
  if ( tree_validate(root, root) == false ) {
    ERROR_PRINTF((ERR_FIL, "Bad pre-remove validation, case 0x%08x\n",
		  whichcase));
    assert (false);
  }
#endif
#endif

  TREE_NIL->color = TREE_BLACK;
  
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
    y = tree_succ(z);
  }

  assert(y != TREE_NIL);

  /* We now know that y has at most one child.  Make x point to that */
  /* child */

  x = (y->left != TREE_NIL) ? y->left : y->right;

  x->parent = y->parent;	/* OKAY if X is TREE_NIL per CLR p. 273 */

  /* If y was the root, have to update the tree. */
  if (y->parent == TREE_NIL) {
    whichcase |= 0x10;
    root = x;
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
    if ( tree_contains(root, y) ) {
      VERB_PRINTF((VERB_FIL, "x=0x%08x, y=0x%08x, z=0x%08x root=0x%08x\n", x,
	     y, z, root));
      VERB_PRINTF((VERB_FIL, "Deleted node 0x%08x still referenced before fixup!\n",
	     y));
    }
  }
#endif

  /* Until I actually understand the fixup transformation, do this */
  /* just as the book would have us do and then fix the fact that the */
  /* wrong thing was deleted after the fact. */
  
  {
    TREEKEY zvalue = z->value;	/* SHAP */

    if (y != z) {
      z->value = y->value;
    }
  
    if ( y->color == TREE_BLACK )
      root = rb_remove_fixup(root, x);

#ifndef NDEBUG
    if ( tree_contains(root, (y!=z) ? y : z) )
      VERB_PRINTF((VERB_FIL, "Deleted znode 0x%08x still referenced by after fixup!\n",
	     z));

    if ( tree_validate(root, root) == false ) {
      ERROR_PRINTF((ERR_FIL, "Bad post-remove validation, case 0x%08x\n",
	      whichcase));
      assert (false);
    }
#endif

    if (y != z) {
#ifndef NDEBUG
      if ( tree_contains(root, y) )
	VERB_PRINTF((VERB_FIL, "Deleted ynode (post fixup) 0x%08x still referenced by after fixup!\n",
	       z));
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
      
      if (root == z)
	root = y;
    }      
  }
#ifndef NDEBUG
  if ( tree_contains(root, z) )
    VERB_PRINTF((VERB_FIL, "Deleted znode (post fixup) 0x%08x still referenced by after fixup!\n",
	   z));

  if ( tree_validate(root, root) == false ) {
    ERROR_PRINTF((ERR_FIL, "Bad post-remove validation, case 0x%08x\n",
	    whichcase));
    assert (false);
  }
#endif
  
  return root;
}

#else
TREENODE *
tree_remove(TREENODE *root, TREENODE *z)
{
#ifdef VERBOSE
  VERB_PRINTF((VERB_FIL, "Remove node 0x%08x from 0x%08x\n", z, root));
#endif
  
  /* Three cases: */
  /*    1.  Z has no children.  This case is trivial. */
  /*    2.  Z has only one child.  This case is trivial. */
  /*    3. Z has two children.  In this case, Let S=TREE-SUCCESSOR(z). */
  /*       S is either a leaf or a node of the form: */
  /* */
  /*                            S */
  /*                      nil     right-subtree */
  /* */
  /*       If S is a leaf, move it to X's current position. */
  /*       If S has a right-subtree, chop out S, splicing it's right */
  /*         subtree into the old position of S, and then move S */
  /*         to the position previously occupied by X. */

  TREENODE *tz = z;
  TREENODE *tzl = z->left;
  TREENODE *tzr = z->right;
  
  int whichcase = 0;
  
  assert ( tree_validate(root, root) );

  if (z->left == TREE_NIL && z->right == TREE_NIL) {
    whichcase = 0;
    if (z->parent == TREE_NIL)
      root = TREE_NIL;
    else if (z->parent->left == z)
      z->parent->left = TREE_NIL;
    else
      z->parent->right = TREE_NIL;
  }
  else if (z->left == TREE_NIL) {
    whichcase = 1;
    if (z->parent == TREE_NIL) {
      root = z->right;
      root->parent = TREE_NIL;
    }
    else if (z->parent->left == z) {
      z->parent->left = z->right;
      z->right->parent = z->parent;
    }
    else {
      z->parent->right = z->right;
      z->right->parent = z->parent;
    }
  }
  else if (z->right == TREE_NIL) {
    whichcase = 2;
    if (z->parent == TREE_NIL) {
      root = z->left;
      root->parent = TREE_NIL;
    }
    else if (z->parent->left == z) {
      z->parent->left = z->left;
      z->left->parent = z->parent;
    }
    else {
      z->parent->right = z->left;
      z->left->parent = z->parent;
    }
  }
  else {
    TREENODE *s = tree_succ(z);

    whichcase = 3;
    /* The painful case. */

    assert (s != root);
    assert (s != TREE_NIL);
    assert (s->left == TREE_NIL);
    assert (s->parent != TREE_NIL);
    
    if (s->right) {
      whichcase = 4;
      s->right->parent = s->parent;
    }

    /* Pull S out of it's current position -- this works if S has no */
    /* children too. */
    if (s->parent->right == s)
      s->parent->right = s->right;
    else
      s->parent->left = s->right;

#ifndef NDEBUG
    assert ( tree_contains(root, s) == false );
#endif

    /* S is now a node with no children which is no longer referenced */
    /* by the tree. */
    s->parent = z->parent;
    if (z->parent == TREE_NIL)
      root = s;
    else if (z->parent->left == z)
      z->parent->left = s;
    else
      z->parent->right = s;

    s->left = z->left;
    if (s->left != TREE_NIL)
      s->left->parent = s;

    s->right = z->right;
    if (s->right != TREE_NIL)
      s->right->parent = s;
  }
	   
#ifndef NDEBUG
  assert ( tree_contains(root, z) == false );
#endif

#ifndef NDEBUG
  if ( tree_validate(root, root) == false ) {
    ERROR_PRINTF((ERR_FIL, "Bad post-remove validation, case %d\n",
	    whichcase));
    assert (false);
  }
#endif

  return root;
}
#endif
