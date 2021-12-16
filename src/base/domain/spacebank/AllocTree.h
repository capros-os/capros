/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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


#ifndef ALLOCTREE_H__
#define ALLOCTREE_H__

#define SB_FRAMES_PER_TREENODE 8
#define SB_OBJECTS_PER_TREENODE (EROS_OBJECTS_PER_FRAME*SB_FRAMES_PER_TREENODE)

typedef struct TREENODE_s TREENODE;

typedef struct TREE_s TREE;

struct TREE_s {
  TREENODE *root;

  /* optimization */
  TREENODE *lastInsert;
  TREENODE *lastRemove;
};

uint32_t
allocTree_Init(TREE *tree);
/* allocTree_Init:
 *   initializes /tree/ to be an empty tree.
 *
 *   Returns 0 (INIT_OK) on success.
 */

uint32_t
allocTree_insertOIDs(TREE *tree, uint8_t type, OID oid, uint32_t count);
/* allocTree_insertOIDs:
 *     Inserts the OIDs in the range [/OID/,OID+/count/) into /tree/.
 *   If necessary, allocates internal node to hold the new oid(s).
 *
 *     Returns 1 on success, 0 if TreeNode space is exhausted.
 *
 *     All of the inserted oid's must fall within the same object
 *   frame.
 */

uint32_t
allocTree_checkForOID(TREE *tree, OID oid);
/* allocTree_checkForOID:
 *     Returns non-zero iff /oid/ is a member of /tree/
 */

struct Bank; /* for type-checking */

uint32_t
allocTree_removeOID(TREE *tree, struct Bank *bank, uint8_t type, OID oid);
/* allocTree_removeOID:
 *     marks the frame containing /oid/ in /tree/ as deallocated. This
 *   function should not be called more than once for each object in
 *   each allocated frame.
 *
 *     Returns:
 *       0 on failure: object is not within tree.
 *       1 on success
 *
 *   if appropriate, deallocates internal tree node.
 *
 *   NOTE: If deallocating the last object in a given frame,
 *   returns the frame to the master free frame list.
 */

uint32_t
allocTree_mergeTrees(TREE *dest, TREE *source);
/* allocTree_mergeTrees:
 *     moves all of the storage allocated in /source/ into /dest/,
 *   destroying /source/ in the process.
 *
 *     returns 0 on success
 */

bool
allocTree_findOID(TREE * tree, OID * pOID);
/* allocTree_findOID: 
 *     find some OID (any OID) contained in this allocTree.
 *
 *      Returns true if an OID is found, false if tree is empty. 
 */

#endif /*ALLOCTREE_H__*/
