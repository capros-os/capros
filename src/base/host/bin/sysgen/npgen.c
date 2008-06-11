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

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <disk/NPODescr.h>

#include <erosimg/App.h>
#include <erosimg/Parse.h>
#include <erosimg/Volume.h>
#include <erosimg/ErosImage.h>

const char * binName;
const char * erosimage;
FILE *map_file = 0;
OID OIDBase = 0;
unsigned long numFramesInRange = 0;

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

      key->u.unprep.oid = oid;
    }
    // else oid is for a phys page, don't change it
  }
  else if (keyBits_IsNodeKeyType(key)) {
    OID oid = key->u.unprep.oid;
    OID frame = oid / DISK_NODES_PER_PAGE;
    OID offset = oid % DISK_NODES_PER_PAGE;
    oid = nodeBase + FrameObIndexToOID(frame, offset);

    key->u.unprep.oid = oid;
  }
}

FILE * binfd;
uint64_t buf[EROS_PAGE_SIZE / sizeof(uint64_t)];
unsigned int nodesInBuf = 0;

void
WriteNode(DiskNode * dn)
{
  memcpy(&((DiskNode *)buf)[nodesInBuf], dn, sizeof (*dn));

  if (++nodesInBuf >= DISK_NODES_PER_PAGE) {
    int s = fwrite(buf, EROS_PAGE_SIZE, 1, binfd);
    if (s != 1)
      diag_fatal(1, "Error writing \"%s\"\n", binName); 
    nodesInBuf = 0;
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
  int i;
  unsigned ndx;
  OID nodeBase, pageBase;
  bool append = false;
  bool nplink = false;
  size_t cnt;

  app_Init("sysgen");

  while ((c = getopt(argc, argv, "apm:b:s:")) != -1) {
    switch(c) {
    case 'a':
      append = true;
      break;

    case 'p':
      nplink = true;
      break;

    case 'm':
      map_file = fopen(optarg, "w");
      if (map_file == NULL)
	return 0;
      
      app_AddTarget(optarg);
      break;

    case 'b':
      i = sscanf(optarg, "%lli", &OIDBase);
      if (i != 1)
        opterr = true;
      break;

    case 's':
      i = sscanf(optarg, "%li", &numFramesInRange);
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
  
  if (opterr || !numFramesInRange)
    diag_fatal(1, "Usage: npgen -s numFramesInRange [-m mapfile] [-b oidbase] eros-image link.bin\n");
  
  erosimage = argv[0];
  binName = argv[1];
  
  image = ei_create();
  ei_ReadFromFile(image, erosimage);

  if (append) {
    binfd = fopen(binName, "r+");
    if (! binfd)
      diag_fatal(1, "Could not open \"%s\"\n", binName); 

    // Indicate that there is an additional preload image in this file.
    struct {
      struct NPObjectsDescriptor NPODescr;
      char filler[EROS_PAGE_SIZE - sizeof(struct NPObjectsDescriptor)];
    } firstFrame;
    cnt = fread(&firstFrame, EROS_PAGE_SIZE, 1, binfd);
    if (cnt != 1)
      diag_fatal(1, "Error reading \"%s\"\n", binName); 

    firstFrame.NPODescr.numPreloadImages++;

    // Rewrite the first frame.
    int err = fseek(binfd, 0, SEEK_SET);
    if (err)
      diag_fatal(1, "Error %d seeking on \"%s\"\n", err, binName); 
    cnt = fwrite(&firstFrame, EROS_PAGE_SIZE, 1, binfd);
    if (cnt != 1)
      diag_fatal(1, "Error rewriting \"%s\"\n", binName); 

    // Append our data.
    err = fseek(binfd, 0, SEEK_END);
    if (err)
      diag_fatal(1, "Error %d seeking on \"%s\"\n", err, binName); 
  } else {
    binfd = fopen(binName, "w");
    if (! binfd)
      diag_fatal(1, "Could not open \"%s\"\n", binName); 
  }

  KeyBits rk;		/* range key */
  KeyBits nk;		/* node key */

  init_RangeKey(&rk, OIDBase, OIDBase + FrameToOID(numFramesInRange));
  // volsize is always the first node in the image:
  init_NodeKey(&nk, (OID) 0, 0);
  // Set slot volsize_range of the volsize node to the range cap:
  ei_SetNodeSlot(image, nk, volsize_range, rk);

#define DIVRNDUP(x,y) (((x) + (y) - 1)/(y))
  /* Allocate one bit per frame in the range. */
  uint32_t numSubMapFrames = DIVRNDUP(numFramesInRange, 8*EROS_PAGE_SIZE);

  uint32_t nPages = image->hdr.nPages;	// nonzero pages
  uint32_t nZeroPages = image->hdr.nZeroPages;
  uint32_t nNodes = image->hdr.nNodes;

  uint32_t nNodeFrames = DIVRNDUP(nNodes, DISK_NODES_PER_PAGE);
#undef DIVRNDUP

  uint32_t framesNeeded = numSubMapFrames + nNodeFrames + nPages + nZeroPages;
  if (numFramesInRange < framesNeeded)
    diag_fatal(1, "You specified -s %d, %s requires at least %d\n",
               numFramesInRange, erosimage, framesNeeded);

  /* All we need to do here is make sure that we pre-allocate the right
   * number of frames so that when the space bank initializes the free
   * frame list it won't step on anything important.
   */
  
  nodeBase = OIDBase;
  pageBase = nodeBase + FrameToOID(nNodeFrames);

  OID IPLOID = 0;
  {
    KeyBits k;
    keyBits_InitToVoid(&k);
    if (ei_GetDirEnt(image, ":ipl:", &k)) {
      OID oldOID = k.u.unprep.oid;

      RelocateKey(&k, nodeBase, pageBase, nPages);
      IPLOID = k.u.unprep.oid;

      if (map_file != NULL)
	fprintf(map_file,
                "image :ipl: node ndx 0x%08lx => disk node oid %#llx\n",
		(uint32_t) oldOID,
		k.u.unprep.oid);
    }
    else
      if (! append)
        diag_printf("Warning: no running domains!\n");
      // If appending, only the original image needs an IPL process.
  }

#if 0
  printf("%d nodes (%d frames), %d pages, %d zpages, %d submaps,"
         " %d free, %d total\n",
         nNodes, nNodeFrames, nPages, nZeroPages, numSubMapFrames, 
         numFramesInRange - framesNeeded, numFramesInRange);
#endif

  struct {
    struct NPObjectsDescriptor NPODescr;
    char filler[EROS_PAGE_SIZE - sizeof(struct NPObjectsDescriptor)];
  } firstFrame = {
    .NPODescr = {
      .OIDBase = OIDBase,
      .IPLOID = IPLOID,
      .numFramesInRange = numFramesInRange,
      .numFrames = nPages + nNodeFrames,
      .numPreloadImages = 1,
      .numNodes = nNodes,
      .numNonzeroPages = nPages
    }
  };

  cnt = fwrite(&firstFrame, EROS_PAGE_SIZE, 1, binfd);
  if (cnt != 1)
    diag_fatal(1, "Error writing \"%s\"\n", binName); 

  /* Copy all of the nodes, relocating the page key and node key
   * OID's appropriately:
   */
  unsigned slot;
  for (ndx = 0; ndx < nNodes; ndx++) {
    DiskNode node;
    OID frame, offset;

    ei_GetNodeContent(image, ndx, &node);

    /* Relocate keys: */
    for (slot = 0; slot < EROS_NODE_SIZE; slot++) {
      RelocateKey(&node.slot[slot], nodeBase, pageBase, nPages);
    }

    if (ndx == 0) {
      // Set up slot volsize_pvolsize of this volsize node.
      init_NodeKey(&node.slot[volsize_pvolsize],
        nplink ? PVOLSIZE_OID	// Link this volsize to persistent volsize
               : OIDBase,	// link to our own volsize
        0);
    }
    
    frame = ndx / DISK_NODES_PER_PAGE;
    offset = ndx % DISK_NODES_PER_PAGE;
    OID oid = FrameObIndexToOID(frame, offset) + nodeBase;

    node.oid = oid;
    
    WriteNode(&node);

    if (map_file != NULL)
      fprintf(map_file, "image node ndx 0x%lx => disk node oid %#llx\n",
	      ndx, oid);
  }

  // Fill out the last frame:
  DiskNode nullNode;
  nullNode.nodeData = 0;
  for (slot = 0; slot < EROS_NODE_SIZE; slot++) {
    keyBits_InitToVoid(&nullNode.slot[slot]);
  }
  while (nodesInBuf) {
    WriteNode(&nullNode);
  }

  /* Write the nonzero pages: */
  for (ndx = 0; ndx < nPages; ndx++) {
    ei_GetDataPageContent(image, ndx, buf);

    cnt = fwrite(buf, EROS_PAGE_SIZE, 1, binfd);
    if (cnt != 1)
      diag_fatal(1, "Error writing \"%s\"\n", binName); 

    OID oid = FrameObIndexToOID(ndx, 0) + pageBase;
    if (map_file != NULL)
      fprintf(map_file, "image dpage ndx 0x%lx => disk page oid %#llx\n",
	      ndx, oid);
  }
  
  if (map_file != NULL)
    fclose(map_file);
  
  ei_destroy(image);
  free(image);

  fclose(binfd);

  app_Exit();
  exit(0);
}
