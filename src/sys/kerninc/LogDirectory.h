#ifndef LOGDIRECTORY_H
#define LOGDIRECTORY_H
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

/** \file LogDirectory.h
    \brief CapROS Kernel Log Directory Interface.

    The object directory keeps track of the most recent instance of
    objects written to the log. In the descriptions below, this location
    is called the "Primary Location".

    It will, in addition, keep up an additional location for the object
    to allow the journalize write operation to locate the version which
    will become current should a restart occur before another checkpoint.

    It provides operations to:
      1. Record new locations for objects
      2. Find the primary location for an object.
      3. Find all the objects whose primary locations are part of a 
         particular checkpoint generation.
      4. Remove all the entries of a particular generation and earlier
         generations.
      5. Find the most recent location for an object which is earlier
         than a specified generation number.

*/

extern LID logCursor;	// next place to write in the main log
extern LID logWrapPoint;	// end of main log

/* oldestNonRetiredGenLid is the LID following the last LID of the
 * newest retired generation. */
extern LID oldestNonRetiredGenLid;

/* workingGenFirstLid is the LID of the first frame of the
 * working generation. */
extern LID workingGenFirstLid;

#define LOG_LIMIT_PERCENT_DENOMINATOR 256
/* logSizeLimited is the size of the main log times the limit percent. */
extern frame_t logSizeLimited;

extern GenNum workingGenerationNumber;

/** The  number of  Log Directory entries.

    This field is filled in by kern_ObjectCache.c and used by
    the log directory.
*/
extern unsigned long numLogDirEntries;


/** The object information maintained by the Log Directory.
 */
typedef struct ObjectDescriptor {
  OID       oid;                /**<The Object ID of the object. */
  ObCount   allocCount;         /**<The allocation count of the object. */
  ObCount   callCount;          /**<The call count of the object. */
  LID       logLoc;             /**<The Log location ID where the object
				   is stored in the log. */
  uint8_t   type;               /**<The base type of the object:
			capros_Range_otPage or capros_Range_otNode. */
} ObjectDescriptor;


/** Record the location of an object.

    The call includes the generation number so it may be used during
    restart. Object locations should be recorded in ascending order of
    generation number.

    Note: This routine invalidates all pointers returned by ld_findObject,
    ld_findFirstObject, or ld_findNextObject.

    @param[in] od The Object Descriptor for the object.
    @param[in] generation The log generation of the object.
*/
void
ld_recordLocation(const ObjectDescriptor *od, GenNum generation);

/** Find an object in the directory.

    This routine will return the primary location LID of the object
    in the working generation, the restart generation, or any of
    the earlier generations still in the directory.

    @param[in] oid The object ID to be located.
    @return A pointer to the ObjectDescriptor for the object or NULL if the
            object is not in the log. This pointer will be good until
	    a change is made to the log directory, either adding or
	    modifing an entry, or deleting a generation.
*/
const ObjectDescriptor *
ld_findObject(OID oid);


/** Find an object for a journalize write.

    This routine will the most recent location LID for the object
    if and only if: 
      (1) It is older than the given generation.
      (2) It is younger than the most recent generation specified in a
          call to ld_generationRetired().
    If there is no location meeting these requirements, it will return NULL.

    @param[in] oid The object ID to be located.
    @param[in] generation The generation the object must be older than.
    @return A pointer to the LID for the object or NULL if the object
            is not in the log, the log entry is younger or equal in age
	    to generation, or the entry is older than the most recent
	    retired generation. This pointer will be good until
	    a change is made to the log directory, adding, deleting, or
	    modifing an entry, or deleting a generation.
*/
const LID *
ld_findObjectForJournal(OID oid, GenNum generation);


/** Find the first object of a generation.

    This routine starts a scan of all objects in a generation.
    ld_nextObject continues the scan. There may be up to one scan in
    progress at any time for any particular generation. Only objects
    whose primary location is in the given generation will be returned.

    Note the the order of objects in a generation is undefined. If it needs
    to be defined for some reason, like optimizing migration, then that
    need will be an additional constraint on the implementation of the
    object directory or the use of the returned values.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the first object in a generation scan
            or NULL. This pointer will be good until
	    a change is made to the log directory, either adding or
	    modifing an entry, or deleting a generation.
*/
const ObjectDescriptor *
ld_findFirstObject(GenNum generation);

/** Find the next object of a generation.

    This routine continues the scan of all objects whose primary location
    is in a generation. See ld_findFirstObject for more information.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the next object in a generation scan
            or NULL. This pointer will be good until
	    a change is made to the log directory, either adding or
	    modifing an entry, or deleting a generation.
*/
const ObjectDescriptor *
ld_findNextObject(GenNum generation);

/** Remove all the objects in a generation, and all earlier generations,
    from the Log Directory.

    Note: This routine may need to be executed in smaller pieces to meet
    real-time requirements.

    Note: This routine invalidates all pointers returned by ld_findObject,
    ld_findOldObject, ld_findFirstObject, or ld_findNextObject.

    @param[in] generation The generation to clear.
*/
void
ld_clearGeneration(GenNum generation);


/** Inform the Log Directory that a generation has been migrated, and a
    checkpoint written which records that fact.

    The Log Directory is free to re-use entries referring to retired
    generations should space be needed for recording more recent objects
    in the directory.

    @param[in] generation The generation which has been retired.
*/
void
ld_generationRetired(GenNum generation);


/** Return the number of available log directory entries.

    A directory entry is considered available if it is either:
      (a) On the free list, or
      (b) Used to record a member of a retired generation.

    @return The number of available directory entries.
*/
unsigned long
ld_numAvailableEntries(void);


/** Return the number of OIDs in the working generation.

    The Log Directory considers the highest numbered generation to be
    the working generation. After a demarcation event, the checkpoint
    logic stabilizes the working generation. Any objects which are 
    altered during this time period are part of the next generation.
    When the stabilization event occurs which commits the checkpoint,
    these objects become logically part of the new working generation.
    The Log Directory will not notice that this event has occured until
    the first entry for the new generation is recorded with
    ld_recordLocation.

    @return The number of entries in the most recent generation recorded
            in the log directory.
*/
unsigned long
ld_numWorkingEntries(void);


/** Remove a log directory entry.

    This entry is designed for re-typing disk frames (currently between
    holding nodes and holding pages). It removes all information about
    both primary and previous primary locations from the directory.

    @param[in] oid Is the OID to remove.
*/
void
ld_removeObjectEntry(OID oid);


/** Get the size of an individual log directory entry.

    Get the size of the individual storage elements used to record OIDs
    in the log. Used during initialization to have an external procedure
    allocate the space for the log directory.

    @return The size of a single entry in the log directory.
*/
uint32_t
ld_getDirEntrySize(void);


/** Define the storage for the log directory.

    Define the storage to be used by the log directory. It must be large
    enough to contain numLogDirEntries of ld_getDirEntrySize each. The caller
    must fill in numLogDirEntries before calling this routine.

    NOTE: This routine must be called before using the Log Directory. The 
    only entry that may be called before this one is ld_getDirEntrySize.

    @param[in] logDirectory Is the address of the area.
*/
void
ld_defineDirectory(void * logDirectory);

#endif /* LOGDIRECTORY_H */
