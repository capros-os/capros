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

#include "id_tree.h"

#include <domain/domdbg.h>
#include "winsyskeys.h"

#define tree_compare(t0,t1) (((t0)->value < (t1)->value) ? -1 : \
                             (((t0)->value > (t1)->value)? 1 : 0))

#define tree_compare_key(t0,key) (((t0)->value < key)? -1 : \
                                  (((t0)->value > key)? 1 : 0))

#define ERROR_PRINTF(x) kdprintf x
#define ERR_FIL KR_OSTREAM

#define VERB_PRINTF(x) kprintf x
#define VERB_FIL KR_OSTREAM

#include <rbtree/tree_init.c>
#include <rbtree/tree_util.c>
#include <rbtree/tree_find.c>
#include <rbtree/tree_insert.c>
#include <rbtree/tree_validate.c>
#include <rbtree/tree_remove.c>
#include <rbtree/tree_contains.c>
#include <rbtree/tree_succ.c>
#include <rbtree/tree_pred.c>
#include <rbtree/tree_min.c>
#include <rbtree/tree_max.c>

#include <stdlib.h>

TREENODE *
id_tree_create() {

  static bool inited = false;

  TREENODE *tree = (TREENODE *)malloc(sizeof(TREENODE));

  /* Only call tree_init() once */
  if (!inited && (inited = true) == true)
    tree_init();

  tree->left = TREE_NIL;
  tree->right = TREE_NIL;
  tree->parent = TREE_NIL;
  tree->color = TREE_BLACK;
  tree->value = 0;
  tree->data = 0;

  return tree;
}
