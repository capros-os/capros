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
/** \file rbtree.h
    \brief Red/Black Tree Interface

    The Red/Black Tree (R/Btree) is used by the kernel for the swap area
    directory. The algorithm was chosen for it good worst-case runtime.

    The rbnode and rbkey structures are exposed because it is sometimes 
    necessary to build hybrid data structures -- e.g. entities that are
    indexed distinctly in multiple trees. It is HIGHLY inadvisable to
    access this structure directly to perform comparisons, as a
    multiKey tree records the fact of multiple keys in the tree
    structure. As a result, a complete compare cannot be done without
    examining the multikey field.
*/

#define TREE_RED 0
#define TREE_BLACK 1

/** The key field for the tree.

    If custom compare functions are provided when creating the RB
    tree (via rbtree_create), then these fields may be used at the
    programmer's discression. If the built-in compare functions are
    used (rbtree_s_cmp and rbtree_s_cmpkey), the w is compared before
    vp, and if the two w values are equal, the two vp values are treated
    as string pointers, and the strings are compared with strcmp.
*/
typedef struct rbkey rbkey;
struct rbkey {
  const void *vp;
  unsigned long w;
};

/** The node structure for the tree.

    Each entry in the tree has a node describing it and linking
    it to other entries.
*/
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

/** The nil node.

    There is a singleton nil node which ends chains.
*/
extern rbnode tree_nil_node;
#define TREE_NIL (&tree_nil_node)

/** The root of the tree.

    This structue defines the root of the tree. It serves to hold
    the first node to search in the tree (which may change as items
    are added and removed), and holds other housekeeping information.

    The seqNo field is used to mark the order of entry of nodes into 
    the tree. This marking is used to define an arbitrary order if the
    tree is permitted to have duplicate keys (which must be declared
    when the tree is created).
*/
typedef struct rbtree rbtree;
struct rbtree {
  unsigned seqNo;
  bool multiKey;

  rbnode *root;
  int (*cmp)(const rbnode *, const rbnode *);
  int (*cmpkey)(const rbnode *, const rbkey *);
};

/** Create a R/BTree.

    @param[in] cmp A function which compares two rbnodes.
    @param[in] cmpkey A function that compares a rbkey against a rbnode.
    @param[in] multiKey True if this tree is to support duplicate keys.
    @return The new RB tree.
*/
rbtree *rbtree_create(int (*cmp)(const rbnode *, const rbnode *), 
		      int (*cmpkey)(const rbnode *, const rbkey *),
		      bool multiKey);

/** Find the smallest node in the RB tree.

    @param[in] rbtree The tree to search.
    @return The RB tree node which has the smallest key
*/
rbnode *rbtree_min(rbtree *);

/** Find the largest node in the RB tree.

    @param[in] rbtree The tree to search.
    @return The node which has the largest RB tree key
*/
rbnode *rbtree_max(rbtree *);

/** Find the next larger node in the RB tree.

    @param[in] rbnode The node whose successor is wanted.
    @return The RB tree node which has the smallest key larger than rbnode.
*/
rbnode *rbtree_succ(rbnode *);

/** Find the next smaller node in the RB tree.

    @param[in] rbnode The node whose predessor is wanted.
    @return The RB tree node which has the largest key smaller than rbnode.
*/
rbnode *rbtree_pred(rbnode *);

/** Insert a node into the RB tree.

    @param[in] rbtree The RB tree.
    @param[in] rbnode The node to be added.
*/
void rbtree_insert(rbtree*, rbnode *);

/** Remove a node from the RB tree.

    @param[in] rbtree The RB tree.
    @param[in] rbnode The node to be removed.
*/
void rbtree_remove(rbtree*, rbnode *);

/** Find a node into the RB tree.

    @param[in] rbtree The RB tree.
    @param[in] rbkey The key to be located.
    @return is the node which matches the key or NULL
*/
rbnode *rbtree_find(rbtree*, rbkey *);

/** Find if a RB tree contains a node.

    @param[in] rbtree The RB tree.
    @param[in] rbnode The node in question.
    @return is true if the tree contains the node, else false.
*/
bool rbtree_contains(rbtree*, rbnode *);

/** Find if a RB tree is empty.

    @param[in] rbtree The RB tree.
    @return is true if the tree contains no nodes, else false.
*/
bool rbtree_isEmpty(rbtree*);

/** Compare a node with a key.

    @param[in] rbtree The RB tree (to access the correct compare function).
    @param[in] rbnode The node in question.
    @param[in] rbkey The key to compare.
    @return is -1 if the key of rn1 is less than rkey, 0
           if they are equal, and +1 if it is greater.
*/
int rbtree_compare_node_to_key(rbtree*, rbnode *rn1, rbkey *rkey);

/** Compare two RB tree nodes using the tree's defined compare function.
    Note that in the case of a tree which permits duplicate keys, the
    result will reflect the ordering of the nodes in the tree, even if
    the keys in the two nodes are equal. If you want to see if two nodes
    have the same key, use rbtree_compare_node_to_key, pointing at the
    key field of one of the nodes.

    @param[in] rbtree The RB tree (to access the correct compare function).
    @param[in] rn1 The first node to compare.
    @param[in] rn2 The second node to compare.
    @return is -1 if the key of rn1 is less than the key of rn2, 0
           if they are equal, and +1 if it is greater.
*/
int rbtree_compare_nodes(rbtree*, rbnode *rn1, rbnode *rn2);

/** Create a rbnode.

    For situations where the rbnode itself is sufficient.
    @param kvp Pointer for the key value or null.
    @param kw  A word to save in the key value.
    @param data Pointer to the data which will be associated with the node.
*/
rbnode *rbnode_create(const void *kvp, unsigned long kw, const void *data);

#ifndef NDEBUG
/* tree_validate() -- check the subtree of ROOT rooted at NODE for
   consistency */
bool rbtree_validate(rbtree *root, rbnode *node);
#endif

/* RB-Trees containing strings as keys are a particularly common case,
   so provide cmp(), cmpkey() functions for this case: */


/** Compare two RB tree nodes. The w part of the key is compared
    first. If the two w values are equal, then a strcmp is performed
    on the string pointers.

    @param[in] rn1 The first node to compare.
    @param[in] rn2 The second node to compare.
    @return is -1 if the key of rn1 is less than the key of rn2, 0
           if they are equal, and +1 if it is greater.
*/
int rbtree_s_cmp(const rbnode *rn1, const rbnode *rn2);

/** Compare a RB tree node with a key. The w part of the key is compared
    first. If the two w values are equal, then a strcmp is performed
    on the string pointers.

    @param[in] rn1 The first node to compare.
    @param[in] rkey The key to compare it with.
    @return is -1 if the key of rn1 is less than rkey, 0
           if they are equal, and +1 if it is greater.
*/
int rbtree_s_cmpkey(const rbnode *rn1, const rbkey *rkey);

#endif /* COMMON_RBTREE_H */
