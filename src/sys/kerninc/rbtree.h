#ifndef COMMON_RBTREE_H
#define COMMON_RBTREE_H
/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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

/* this RB-Tree code is from the OpenCM code base */

#define TREE_RED 0
#define TREE_BLACK 1

typedef struct rbkey rbkey;
struct rbkey {
  const void *vp;
  unsigned long w;
};

/* The rbnode and rbkey structures are exposed because it is sometimes 
   necessary to build hybrid datas tructures -- e.g. entities that are
   indexed distinctly in multiple trees. It is HIGHLY inadvisable to
   access this structure directly to perform comparisons, as a
   multiKey tree records the fact of multiple keys in the tree
   structure. As a result, a complete compare cannot be done without
   examining the multikey field. */
typedef struct rbnode rbnode;
struct rbnode {
  rbnode *left;
  rbnode *right;
  rbnode *parent;

  unsigned seqNo;
  int color;

  rbkey  value;

  const void *data;
};

/* There is a singleton nil node */
extern rbnode tree_nil_node;
#define TREE_NIL (&tree_nil_node)

typedef struct rbtree rbtree;
struct rbtree {
  unsigned seqNo;
  bool multiKey;

  rbnode *root;
  int (*cmp)(const rbnode *, const rbnode *);
  int (*cmpkey)(const rbnode *, const rbkey *);
};

rbtree *rbtree_create(int (*cmp)(const rbnode *, const rbnode *), 
		      int (*cmpkey)(const rbnode *, const rbkey *),
		      bool multiKey);

rbnode *rbtree_min(rbtree *);
rbnode *rbtree_max(rbtree *);
rbnode *rbtree_succ(rbnode *);
rbnode *rbtree_pred(rbnode *);
void rbtree_insert(rbtree*, rbnode *);
void rbtree_remove(rbtree*, rbnode *);
rbnode *rbtree_find(rbtree*, rbkey *);
bool rbtree_contains(rbtree*, rbnode *);
bool rbtree_isEmpty(rbtree*);

int rbtree_compare_node_to_key(rbtree*, rbnode *, rbkey*);
int rbtree_compare_nodes(rbtree*, rbnode *, rbnode *);

/* For situations where the rbnode itself is sufficient: */
rbnode *rbnode_create(const void *kvp, unsigned long kw, const void *data);

#ifndef NDEBUG
/* tree_validate() -- check the subtree of ROOT rooted at NODE for
   consistency */
bool rbtree_validate(rbtree *root, rbnode *node);
#endif

/* RB-Trees containing strings as keys are a particularly common case,
   so provide cmp(), cmpkey() functions for this case: */
int rbtree_s_cmp(const rbnode *rn1, const rbnode *rn2);
int rbtree_s_cmpkey(const rbnode *rn1, const rbkey *rkey);

#endif /* COMMON_RBTREE_H */
