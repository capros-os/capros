#ifndef __ID_TREE_H__
#define __ID_TREE_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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

#include <eros/target.h>

/* Use an rb tree to store (window id, window pointer) pairs */
#define RB_TREE

typedef uint32_t TREEKEY;
typedef struct TREENODE_s TREENODE;
struct TREENODE_s {
  TREENODE *left;
  TREENODE *right;
  TREENODE *parent;
  uint32_t color : 1;
  TREEKEY value;

  uint32_t data;
};

/* For now: */
#define assert(ignore) ((void) 0)

#include <rbtree/tree.h>

TREENODE *id_tree_create();

#endif
