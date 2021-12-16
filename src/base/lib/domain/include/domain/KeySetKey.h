#ifndef __KEYSETKEY_H__
#define __KEYSETKEY_H__

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

#define OC_KeySet_AddKey              1
#define OC_KeySet_RemoveKey           2
#define OC_KeySet_ContainsKey         3

#define OC_KeySet_IsEmpty             5
#define OC_KeySet_Empty               6

#define OC_KeySet_MakeReadOnlyKey     8

#define OC_KeySet_AddKeysFromSet      9
#define OC_KeySet_RemoveKeysNotInSet 10

#if 1
#define OC_KeySet_IsSubsetOfSet      16
#else
#define OC_KeySet_CompareSets        16
#endif 


#define RC_KeySet_KeyNotInSet         1

#define RC_KeySet_SetInvalid          2
#define RC_KeySet_PassedSetInvalid    3
/* RC_KeySet_SetInvalid is returned if the KeyBits revision has changed
 *since the first key was added to the keyset.  This can be returned at
 *any time, and implies that you must rebuild the keyset from scratch.
 *At that point you should Empty the set, and add everything back.
 */
#define RC_KeySet_SetsEqual           16
#define RC_KeySet_SetContainsOtherSet 17
#define RC_KeySet_OtherSetContainsSet 18
#define RC_KeySet_SetsDisjoint        19
#define RC_KeySet_SetsDifferent       20
#define RC_KeySet_DataMismatch        21
/* These are for the CompareSets order.
 *  DataMismatch can only be returned if asked to compare data fields.
 *
 *  SetsDifferent is returned if none of the other codes apply
 *        (i.e. Set intersect otherset not empty,
 *              Set union otherset is not Set or Otherset)
 */

#ifndef __ASSEMBLER__
uint32_t keyset_add_key(uint32_t krKeySet, 
                        uint32_t krKey, 
                        uint32_t data,/*OUT*/uint32_t *oldData);
uint32_t keyset_remove_key(uint32_t krKeySet, 
                           uint32_t krKey,
                           /*OUT*/uint32_t *data);
uint32_t keyset_contains_key(uint32_t krKeySet, 
                             uint32_t krKey, 
                             /*OUT*/uint32_t *data);

uint32_t keyset_is_empty(uint32_t krKeySet);
uint32_t keyset_empty(uint32_t krKeySet);

uint32_t keyset_make_read_only(uint32_t krKeySet, uint32_t krROKeySet);

uint32_t keyset_add_keys_from_set(uint32_t krKeySet, uint32_t krOtherSet);
uint32_t keyset_remove_keys_not_in_set(uint32_t krKeySet, uint32_t
				       krOtherSet);
#if 1
uint32_t keyset_is_subset_of(uint32_t krKeySet, uint32_t krOtherSet);
#else
uint32_t keyset_compare_sets(uint32_t krKeySet,
                             uint32_t krOtherSet, 
                             uint32_t compareData);
#endif
#endif /* __ASSEMBLER__*/

#endif /* __KEYSETKEY_H__ */
