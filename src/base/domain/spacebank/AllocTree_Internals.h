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


#ifndef ALLOCTREE_INTERNALS_H
#define ALLOCTREE_INTERNALS_H

typedef uint64_t TREEKEY;
typedef struct TREEBODY_s TREEBODY;

struct TREEBODY_s {
#ifndef TREE_NO_TYPES
  uint8_t type[SB_FRAMES_PER_TREENODE];
#endif  
  uint16_t map[SB_FRAMES_PER_TREENODE];  /* contains the number of
					  * allocations in this frame,
					  * or BYTE_MAX if this frame
					  * is not part of this chunk.
					  */
};

struct TREENODE_s {
  TREENODE *left;
  TREENODE *right;
  TREENODE *parent;
  int       color;

  TREEKEY   value;
  TREEBODY  body;
};

#define RB_TREE

#include <rbtree/tree.h>

#if 0
extern int tree_compare(TREENODE *, TREENODE *);
extern int tree_compare_key(TREENODE *, TREEKEY);
extern int tree_body_copy(TREEBODY *to, TREEBODY *from);
#endif

TREENODE *tree_newNode(TREEKEY key);
int tree_deleteNode(TREENODE *node);

#endif /* ALLOCTREE_INTERNALS_H */
