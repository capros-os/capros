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

void tree_init(void);
TREENODE *tree_min(TREENODE *root);
TREENODE *tree_max(TREENODE *root);
TREENODE *tree_succ(TREENODE *root);
TREENODE *tree_pred(TREENODE *root);
TREENODE *tree_insert(TREENODE *root, TREENODE *node);
TREENODE *tree_remove(TREENODE *root, TREENODE *node);
TREENODE *tree_find(TREENODE *root, TREEKEY k);
bool tree_contains(TREENODE *root, TREENODE *elem);

/* tree_validate() -- check the subtree of ROOT rooted at NODE for
   consistency */
bool tree_validate(TREENODE *root, TREENODE *node);

extern int tree_compare(TREENODE *, TREENODE *);
extern int tree_compare_key(TREENODE *, TREEKEY);

#ifdef RB_TREE
#define TREE_RED 0
#define TREE_BLACK 1

extern TREENODE tree_nil_node;
#define TREE_NIL (&tree_nil_node)
#else
#define TREE_NIL 0
#endif
