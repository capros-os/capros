#ifndef __EROSIMAGE_H__
#define __EROSIMAGE_H__
/*
 * Copyright (C) 1998, 1999, 2001, 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

#include <eros/target.h>
#include <erosimg/DiskKey.h>
#include <erosimg/StringPool.h>
#include <erosimg/Intern.h>
#include <erosimg/ExecArch.h>

#define EROS_IMAGE_VERSION 4

/* An EROS Image is a *relocatable* image of a named object.  The
 * image file holds some structured object, identified by the key in
 * the header, and some number of nodes and pages.  The nodes and
 * pages are assumed to be numbered starting at CDA=0 and have rising
 * consecutive CDA's.
 * 
 * Every ErosImage file has a name in it's header, which is the
 * human-readable name of the ErosImage object.  It may someday become
 * useful to allow multiple named objects in an ErosImage file, in
 * which event we shall have to add a table.
 * 
 * Most keys in an ErosImage file are numbered relative to the image
 * file, and have their 'rHazard' bit set to indicate this.  Absolute
 * keys do not have their 'rHazard' bit set.
 * 
 * Pages that hold only zeros are not actually stored in the file.
 * Such pages are written with ascending CDA's beginning at
 *    0x8000 0000 0000
 * 
 * Keys to such pages have their 'rHazard' bit set.
 * 
 * THE FOLLOWING PARAGRAPH IS OBSOLETE, BUT IS RETAINED TO CAPTURE
 * INTENT SO WE CAN REVISIT IT:
 * 
 * ErosImage files may also contain references to objects not included
 * in the image.  Such objects are indicated by name.  Keys to those
 * objects have their 'wHazard' bit set, and the cdalo field of the
 * key gives an index into the string table.  It is expected that the
 * sysgen utility will resolve such references when the ErosImage
 * files are all bound together into a system image.
 * 
 */

typedef struct EiDirent EiDirent;
struct EiDirent {
  uint32_t	name;		/* index into string pool */
  KeyBits	key;
};

void ei_dirent_init(EiDirent *);

typedef struct ErosHeader ErosHeader;
struct ErosHeader {
  char		signature[8];	/* "ErosImg\0" */
  uint32_t 	imageByteSex;	/* 0 == little endian, else big endian */
  uint32_t	version;	/* image file version */
  uint32_t	architecture;

  uint32_t	nDirEnt;	/* number of directory entries */
  uint32_t	dirOffset;	/* location in file of image directory. */

  uint32_t	nStartups;	/* number of startup activities */
  uint32_t	startupsOffset;	/* location in file of startups directory */
  
  uint32_t	nPages;		/* total number of nonzero page images */
  uint32_t	nZeroPages;	/* total number of zero page images */
  uint32_t	pageOffset;	/* location in file of first page description */

  uint32_t	nNodes;		/* total number of node images */
  uint32_t	nodeOffset;	/* location in file of first node description */

  uint32_t	strSize;	/* size of string table */
  uint32_t	strTableOffset;	/* file offset of string table */
};

void erosheader_init(ErosHeader *);

typedef struct ErosImage ErosImage;

struct ErosImage {
  ErosHeader hdr;

  /* protected: */
  StringPool *pool;
  
  uint8_t *pageImages;
  struct DiskNode * nodeImages;
  struct EiDirent *dir;
  struct EiDirent *startupsDir;

  uint32_t maxPage;
  uint32_t maxNode;
  uint32_t maxDir;
  uint32_t maxStartups;
  
#if 0
  void ValidateImage(const char* target);
  bool DoGetPageInSegment(const KeyBits& segRoot,
			  uint64_t segOffset,
			  KeyBits& pageKey);

  KeyBits DoAddPageToBlackSegment(const KeyBits& segRoot,
				  uint64_t segOffset,
				  const KeyBits& pageKey,
				  uint64_t path,
				  bool expandRedSegment);

  KeyBits DoAddSubsegToBlackSegment(const KeyBits& segRoot,
				    uint64_t segOffset,
				    const KeyBits& segKey);

  void DoPrintSegment(uint32_t slot, const KeyBits&, uint32_t indentLevel,
		      const char *annotation, bool startKeyOK) const;
  
  void GrowNodeTable(uint32_t newMax);
  void GrowPageTable(uint32_t newMax);
  void GrowDirTable(uint32_t newMax);
  void GrowStartupsTable(uint32_t newMax);
#endif
};

#ifdef __cplusplus
extern "C" {
#endif
  ErosImage *ei_create();
  void ei_destroy(ErosImage *);

  INLINE const StringPool *
  ei_GetStringPool(const ErosImage *ei)
  { return ei->pool; }

  void ei_SetArchitecture(ErosImage *, enum ExecArchitecture);

  const char *ei_GetString(const ErosImage *, int ndx);

  void ei_WriteToFile(ErosImage *, const char *target);
  void ei_ReadFromFile(ErosImage *, const char *source);

  /* Add the respective objects, returning a key to the object added.
   * Set the write bit in the disk key if so requested.
   */

  KeyBits ei_AddDataPage(ErosImage *, const uint8_t *buf, bool readOnly);
  KeyBits ei_AddZeroDataPage(ErosImage *, bool readOnly);

  KeyBits ei_AddNode(ErosImage *, bool readOnly);
  KeyBits ei_AddProcess(ErosImage *);

  KeyBits ei_AddChain(ErosImage *);
  void ei_AppendToChain(ErosImage *, KeyBits *chain, KeyBits k);

  /* Name is unnecessary -- it is included only for use in listings. */
  bool ei_AddStartup(ErosImage *, const char *name, KeyBits);
  bool ei_GetStartupEnt(ErosImage *, const char *name, KeyBits *);

#if 0
  /* Maybe to be implemented in the future: */
  void ei_GetReserve(ErosImage *, uint32_t index, struct CpuReserve& rsrv);
  void ei_SetReserve(ErosImage *, const struct CpuReserve& rsrv);
#endif

  void ei_AddDirEnt(ErosImage *, const char *name, KeyBits key);
  /* Assign is like add, but will over-write old entry if there is one. */
  void ei_AssignDirEnt(ErosImage *, const char *name, KeyBits key);
  bool ei_GetDirEnt(ErosImage *, const char *name, KeyBits *);
  void ei_SetDirEnt(ErosImage *, const char *name, KeyBits key);
  bool ei_DelDirEnt(ErosImage *, const char *name);

#if 0
  /* Import the contents of another image into this one. */
  void ei_Import(ErosImage *to, const ErosImage *from);
#endif

  /* We cannot just hand out a DiskNode&, because the disk node images
   * move around.  It's easier to manipulate things this way instead:
   */
  void ei_SetNodeSlot(ErosImage *, KeyBits nodeKey, uint32_t slot, KeyBits key);
  KeyBits ei_GetNodeSlot(const ErosImage *, KeyBits nodeKey, uint32_t slot);
  KeyBits ei_GetNodeSlotFromIndex(const ErosImage *, uint32_t nodeNdx, uint32_t slot);
  void ei_SetGPTFlags(ErosImage *ei, KeyBits nodeKey, uint8_t flags);
  uint16_t ei_GetNodeData(ErosImage *ei, KeyBits nodeKey);
  void ei_SetNodeData(ErosImage *ei, KeyBits nodeKey, uint16_t nd);

  /* Construction support for segments.  Given a segment root key and
   * an offset, hand back a new segment root key:
   */
  int ei_GetAnyBlss(const ErosImage *, KeyBits segRoot);
  bool ei_SetBlss(ErosImage *, KeyBits gptKey, unsigned int blss);

  KeyBits ei_AddSegmentToSegment(ErosImage *, KeyBits segRoot,
				 uint64_t segOffset,
				 KeyBits pageKey);

  KeyBits ei_AddSubsegToSegment(ErosImage *, KeyBits segRoot,
				uint64_t segOffset,
				KeyBits pageKey);

  bool ei_GetPageInSegment(ErosImage *, KeyBits segRoot,
			   uint64_t segOffset,
			   KeyBits *pageKey);

  uint8_t * ei_GetPageContentRef(ErosImage *, KeyBits *pageKey);
  void ei_SetProcessState(ErosImage *, KeyBits procRoot, uint8_t state);
  uint8_t ei_GetProcessState(ErosImage *, KeyBits procRoot);

  void ei_PrintDomain(const ErosImage *ei, KeyBits);
  void ei_PrintSegment(const ErosImage *ei, KeyBits);
  void ei_PrintNode(const ErosImage *ei, KeyBits);
  void ei_PrintPage(const ErosImage *ei, KeyBits);

  /* Return by value, since directory can move: */
  INLINE EiDirent
  ei_GetDirEntByIndex(const ErosImage *ei, uint32_t ndx)
  { return ei->dir[ndx]; }

  INLINE EiDirent
  ei_GetStartupEntByIndex(const ErosImage *ei, uint32_t ndx)
  { return ei->startupsDir[ndx]; }

  void ei_GetDataPageContent(const ErosImage *, uint32_t pageNdx, void * buf);
  void ei_GetNodeContent(const ErosImage *, uint32_t nodeNdx, DiskNode * node);

#ifdef __cplusplus
}
#endif

#endif /* __EROSIMAGE_H__ */
