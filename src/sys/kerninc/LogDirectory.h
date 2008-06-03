#ifndef OBJECTDIRECTORY_H
#define OBJECTDIRECTORY_H
/*
 * Copyright (C) 2008, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */


#include <kerninc/kernel.h>
typedef OID LID; /* Temporary hack until LID is defined */

/** \file LogDirectory.h
    \brief CapROS Kernel Object Directory Interface.

    The object directory keeps track of the most recent instance of
    objects written to the log. It will keep up to two locations, one
    in the working generation, if there is a working generation, and
    the most recent version in the restart generation, unmigrated 
    generation(s), migrated but not retired generation(s), and the
    retired generation(s).

    It provides operations to:
      1. Record new locations for objects
      2. Find the most recent location for an object.
      3. Find all the objects in a particular checkpoint generation.
      4. Remove all the entries of a particular checkpoint generations.

*/
/** The object information maintained by the Object Directory.
 */
typedef struct ObjectDescriptor {
  OID       oid;                /**<The Object ID of the object. */
  ObCount   allocCount;         /**<The allocation count of the object. */
  ObCount   callCount;          /**<The call count of the object. */
  LID       logLoc;             /**<The Log location ID where the object
				   is stored in the log. */
  uint8_t   allocCountUsed : 1; /**<TRUE if the allocation count is stored
				   in some other object. */
  uint8_t   callCountUsed : 1;  /**<TRUE if the call count is stored in
				   some other object. */
  uint8_t   type : 6;           /**<The type of the object. */
} ObjectDescriptor;

/** Record the location of an object.

    The call includes the generation number so it may be used during
    restart.

    @param[in] od The Object Descriptor for the object.
    @param[in] lid The location of the object in the checkpoint log.
    @param[in] generation The log generation of the object.
*/
void od_recordLocation(ObjectDescriptor *od, LID lid, uint64_t generation);

/** Find an object in the directory.

    This routine will return the LID of most recent version of the object
    in the working generation, the restart generation, or any of the earlier
    generations still in the directory.

    @param[in] oid The object ID to be located.
    @param[in] type The type of the object.
    @return A pointer to the ObjectDescriptor for the object or NULL if the
            object is not in the log.
*/
ObjectDescriptor *od_findObject(OID oid, uint8_t type);

/** Find the first object of a generation.

    This routine starts the scan of all objects in a generation.
    od_nextObject continues the scan. There may be up to two scans in
    progress at any time (one for the checkpoint routines and one for
    migration). They are separated by using different generation numbers.

    Note the the order of objects in a generation is undefined. If it needs
    to be defined for some reason, like optimizing migration, then that
    need will be an additional constraint on the implementation of the
    object directory.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the first object in a generation scan.
*/
ObjectDescriptor *od_findFirstObject(uint64_t generation);

/** Find the next object of a generation.

    This routine continues the scan of all objects in a generation.
    See od_findFirstObject for more information.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the next object in a generation scan.
*/
ObjectDescriptor *od_findNextObject(uint64_t generation);

/** Remove all the objects in a generation from the Object Directory.

    Note: This routine may need to be executed in smaller pieces to meet
    real-time requirements.

    @param uint64_t generation The generation to clear.
*/
void od_clearGeneration(uint64_t generation);

#endif /* OBJECTDIRECTORY_H */
