/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, 2008, Strawberry Development Group.
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

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <disk/PagePot.h>
#include <disk/NPODescr.h>

#include <erosimg/App.h>
#include <erosimg/Parse.h>
#include <erosimg/Volume.h>
#include <erosimg/ErosImage.h>
	  
#define DIVRNDUP(x,y) (((x) + (y) - 1)/(y))

Volume *pVol;

const char* targname;
const char* erosimage;
FILE *map_file = 0;
OID OIDBase;

void
RelocateKey(KeyBits *key, OID nodeBase, OID pageBase,
	    uint32_t nPages)
{
  if ( keyBits_IsType(key, KKT_Page) ) {
    if (key->u.unprep.oid < OID_RESERVED_PHYSRANGE) {
      OID oid = pageBase + (key->u.unprep.oid * EROS_OBJECTS_PER_FRAME);

      if (keyBits_IsPrepared(key)) {
        keyBits_SetUnprepared(key);
        oid += (nPages * EROS_OBJECTS_PER_FRAME);
      }

      assert (oid < 0x100000000llu);
    
      key->u.unprep.oid = oid;
    }
    // else oid is for a phys page, don't change it
  }
  else if (keyBits_IsNodeKeyType(key)) {
    OID oid = key->u.unprep.oid;
    OID frame = oid / DISK_NODES_PER_PAGE;
    OID offset = oid % DISK_NODES_PER_PAGE;
    frame *= EROS_OBJECTS_PER_FRAME;
    frame += nodeBase; /* JONADAMS: add in the node base */
    oid = frame + offset;

    assert (oid < 0x100000000llu);

    key->u.unprep.oid = oid;
  }
}

int
main(int argc, char *argv[])
{
  int c;
  extern int optind;
  extern char *optarg;
  bool opterr = false;
  ErosImage *image;
  uint32_t nObjectRange = 0;
  int i;
  uint32_t nPages, nZeroPages, nNodes;
  unsigned ndx;
  unsigned int drive, partition;
  OID nodeBase, pageBase;

  app_Init("sysgen");

  while ((c = getopt(argc, argv, "m:b:")) != -1) {
    switch(c) {
    case 'm':
      map_file = fopen(optarg, "w");
      if (map_file == NULL)
	return 0;
      
      app_AddTarget(optarg);
      break;

    case 'b':
      // OIDBase ought to be FIRST_PERSISTENT_OID.
      i = sscanf(optarg, "%lli", &OIDBase);
      if (i != 1)
        opterr = true;
      break;

    default:
      opterr = true;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argc != 2)
    opterr = true;
  
  if (opterr)
    diag_fatal(1, "Usage: sysgen [-m mapfile] [-b oidbase] volume-file eros-image\n");
  
  targname = argv[0];
  erosimage = argv[1];

  {
    /* Kluge to derive boot volume drive and partition from target name. */
    /* Assume last character of the name is a digit. */
    char n;
    int len = strlen(targname);
    if (len < 3)
      diag_fatal(1, "target file name too short\n");

    n = targname[len-1];
    if (n < '0' || n > '9')
      diag_fatal(1, "target file name must end in a digit\n");

    if (targname[len-3] == 'f') {	/* fdn */
      drive = 0x00 + (n-'0');
      partition = 0;
    } else if (targname[len-3] == 'd'
               || targname[len-3] == 'b' ) {	/* hdxn or nbdn */
      if (n == '0')
        diag_fatal(1, "Hard disk partion must start with 1\n");

      drive = 0x80 + (targname[len-2]-'a');
      partition = n-'1';
    } else
      diag_fatal(1, "Target file name must be 'hdxn' or 'nbdn' or 'fdn'\n");
  }
  
  pVol = vol_Open(targname, true, 0, 0,
                  (drive << 24) + (partition << 16) );
  if ( !pVol )
    diag_fatal(1, "Could not open \"%s\"\n", targname);
  
  vol_ResetVolume(pVol);
  
  image = ei_create();
  ei_ReadFromFile(image, erosimage);

  for (i = 0; i < vol_MaxDiv(pVol); i++) {
    const Division* d = vol_GetDivision(pVol, i);
    if (d->type == dt_Object) {
      KeyBits rk;		/* range key */
      KeyBits nk;		/* node key to persistent volsize node */

      init_RangeKey(&rk, d->startOid, d->endOid);
      init_NodeKey(&nk, (OID) 0, 0);

      if (d->startOid == FIRST_PERSISTENT_OID) {
        ei_SetNodeSlot(image, nk, volsize_range, rk);
	if (d->endOid - d->startOid >= (uint64_t) UINT32_MAX)
	  diag_fatal(1, "Object range too large for sysgen\n");
      } else {
        if (3 + nObjectRange >= volsize_range)
	  diag_fatal(1, "Too many ranges for volsize\n");
        ei_SetNodeSlot(image, nk, 3 + nObjectRange, rk);
      }

      nObjectRange++;
    }
  }
    
  nPages = image->hdr.nPages;
  nZeroPages = image->hdr.nZeroPages;
  nNodes = image->hdr.nNodes;

  uint32_t nodeFrames = DIVRNDUP(nNodes, DISK_NODES_PER_PAGE);

  /* All we need to do here is make sure that we pre-allocate the right
   * number of frames so that when the space bank initializes the free
   * frame list it won't step on anything important.
   */
  
  nodeBase = OIDBase;
  pageBase = nodeBase + FrameToOID(nodeFrames);

  /* Copy all of the nodes, relocating the page key and node key
   * OID's appropriately:
   */
  for (ndx = 0; ndx < nNodes; ndx++) {
    unsigned slot;
    DiskNodeStruct node;
    OID frame, offset;

    ei_GetNodeContent(image, ndx, &node);

    /* Relocate keys: */
    for (slot = 0; slot < EROS_NODE_SIZE; slot++) {
      RelocateKey(&node.slot[slot], nodeBase, pageBase, nPages);
    }
    
    frame = ndx / DISK_NODES_PER_PAGE;
    offset = ndx % DISK_NODES_PER_PAGE;
    OID oid = FrameObIndexToOID(frame, offset) + nodeBase;

    node.oid = oid;
    
    vol_WriteNode(pVol, oid, &node);

    if (map_file != NULL)
      fprintf(map_file, "image node ndx 0x%lx => disk node oid %#llx\n",
	      ndx, oid);
  }

  /* Write the nonzero pages: */
  for (ndx = 0; ndx < nPages; ndx++) {
    uint8_t buf[EROS_PAGE_SIZE];
    
    ei_GetDataPageContent(image, ndx, buf);

    OID oid = FrameObIndexToOID(ndx, 0) + pageBase;
    vol_WriteDataPage(pVol, oid, buf);

    if (map_file != NULL)
      fprintf(map_file, "image dpage ndx 0x%lx => disk page oid %#llx\n",
	      ndx, oid);
  }

  /* Zero the non-contentful pages: */
  for (ndx = 0; ndx < nZeroPages; ndx++) {
    OID oid;
    uint8_t buf[EROS_PAGE_SIZE];

    /* Following is redundant, but useful until zero pages are
     * implemented:
     */
    
    memset(buf, 0, EROS_PAGE_SIZE);
    
    oid = ((ndx + nPages) * EROS_OBJECTS_PER_FRAME) + pageBase;
    vol_WriteDataPage(pVol, oid, buf);

    if (map_file != NULL)
      fprintf(map_file, "image zdpage ndx 0x%lx => disk page oid 0x%08lx%08lx\n",
	      ndx, (uint32_t) (oid >> 32), (uint32_t) oid);

#if 0
    PagePot pagePot;
    pagePot.flags = PagePot::ZeroPage;
    
    pVol->WritePagePotEntry(oid, pagePot);
#endif
  }

  if (map_file != NULL)
    fclose(map_file);

  vol_Close(pVol);
  free(pVol);
  
  ei_destroy(image);
  free(image);

  app_Exit();
  exit(0);
}
