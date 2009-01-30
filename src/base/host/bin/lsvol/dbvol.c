/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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
#include <unistd.h>


#include <erosimg/App.h>
#include <erosimg/Parse.h>
#include <erosimg/ExecImage.h>
#include <erosimg/Volume.h>
#include <erosimg/StringPool.h>

extern void PrintDiskKey(KeyBits);

Volume *pVol;

const char* targname;
const char* sysmap;

uint16_t
ip_cksum(uint16_t *buf, int len)
{
  uint16_t cksum=0;
  typedef char * caddr_t;

  if ((size_t)buf & 0x01)
    {
      cksum=(*(uint8_t *)buf)<<8;
      buf=(uint16_t *)((caddr_t)buf+1);
      len--;
    }
  while (len>1)
    {
      cksum+=*buf++;
      if (cksum & 0x10000)
	cksum=(cksum & 0xFFFF)+1;
      len-=2;
    }
  if (len)
    {
      cksum+=*(uint8_t *)buf;
      if (cksum & 0x10000)
	cksum=(cksum & 0xFFFF)+1;
    }
  return ~cksum;
}

static void
PrintNodeHeader(DiskNode * dn)
{
  diag_printf("Node OID=");
  diag_printOid(get_target_oid(&dn->oid));
  diag_printf(" allocCount=");
  diag_printCount(dn->allocCount);
  diag_printf(" callCount=");
  diag_printCount(dn->callCount);
  diag_printf(" cksum=0x%04x\n", 
	      ip_cksum((uint16_t*)dn, sizeof(DiskNode)));
}

int
ProcessCmd()
{
  char buf[1024];
  const char *fileName;
  OID oid;
  uint32_t w;
  const char* rest;

  if (isatty(0)) fputs("dbvol> ", stdout);
  if (fgets(buf, 1024, stdin) == 0)
    return 0;
  
  buf[1023] = 0;
  
  parse_TrimLine(buf);
  rest = buf;

  if (parse_MatchStart(&rest, buf) &&
      parse_MatchKeyword(&rest, "n") &&
      parse_MatchOIDVal(&rest, &oid) &&
      parse_MatchEOL(&rest) ) {

    DiskNode node;
    if (vol_ReadNode(pVol, oid, &node) ) {
      PrintNodeHeader(&node);

      unsigned i;
      for (i = 0; i < EROS_NODE_SIZE; i++) {
	diag_printf("  [%02d]  ",i);
	PrintDiskKey(node.slot[i]);
	diag_printf("\n");
      }
      diag_printf("\n");
    }
  }
  else if (parse_MatchStart(&rest, buf) &&
	   parse_MatchKeyword(&rest, "rn") &&
	   parse_MatchOIDVal(&rest, &oid) &&
	   parse_MatchEOL(&rest) ) {

    DiskNode node;
    if ( vol_ReadNode(pVol, oid, &node) ) {
      PrintNodeHeader(&node);
      unsigned i;
      uint32_t *wpNode = (uint32_t *) &node;
      uint32_t nWords = sizeof(DiskNode) / sizeof(uint32_t);

      for (i = 0; i < nWords;) {
	unsigned j;
	for (j = 0; (j < 4) && (i < nWords); j++, i++)
	  diag_printf("  0x%08x", wpNode[i]);
	diag_printf("\n");
      }
    }
  }
  else if (parse_MatchStart(&rest, buf) &&
	   parse_MatchKeyword(&rest, "p") &&
	   parse_MatchOIDVal(&rest, &oid) &&
	   parse_MatchEOL(&rest) ) {

    uint8_t buf[EROS_PAGE_SIZE];
    VolPagePot pinfo;

    vol_GetPagePotInfo(pVol, oid, &pinfo);

    diag_printf("Page OID=");
    diag_printOid(oid);
    diag_printf(" tag=%d allocCount=", pinfo.type);
    diag_printCount(pinfo.count);
    diag_printf("\n");

    if ( vol_ReadDataPage(pVol, oid, buf) ) {
      int i;
      uint8_t *bufp = buf;
    
      for (i = 0; i < 8; i++) {
	int j;
	diag_printf(" +0x%04x  ", i * 16);
	for (j = 0; j < 16; j++) {
	  diag_printf("%02x ", *bufp);
	  bufp++;
	}
	diag_printf("\n");
      }
      diag_printf("...\n");
      diag_printf("\n");
    }
  }
  else if (parse_MatchStart(&rest, buf) &&
	   parse_MatchKeyword(&rest, "p") &&
	   parse_MatchOIDVal(&rest, &oid) &&
	   parse_Match(&rest, ",") &&
	   parse_MatchWord(&rest, &w) &&
	   parse_MatchEOL(&rest) ) {

    uint8_t buf[EROS_PAGE_SIZE];
    VolPagePot pinfo;

    vol_GetPagePotInfo(pVol, oid, &pinfo);

    diag_printf("Page OID=");
    diag_printOid(oid);
    diag_printf(" tag=%d allocCount=", pinfo.type);
    diag_printCount(pinfo.count);
    diag_printf("\n");

    if ( vol_ReadDataPage(pVol, oid, buf) ) {
      uint8_t *bufp;
      int i;
#if 0
      diag_printf(" cksum=0x%04x\n",
		   ip_cksum((uint16_t*)buf, EROS_PAGE_SIZE)); 
#endif
      w &= ~0xfu;	/* round down to mult of 16 */
      bufp = buf;
      bufp += w;
    
      for (i = 0; i < 8; i++) {
	int j;
	diag_printf(" +0x%04x  ", w + (i * 16));
	for (j = 0; j < 16; j++) {
	  if ((size_t)(bufp - buf) >= EROS_PAGE_SIZE)
	    continue;
	  diag_printf("%02x ", *bufp);
	  bufp++;
	}
	diag_printf("\n");
      }
      diag_printf("...\n");
      diag_printf("\n");
    }
  }
  else if (parse_MatchStart(&rest, buf) &&
	   parse_MatchKeyword(&rest, "lp") &&
	   parse_MatchOIDVal(&rest, &oid) &&
	   parse_MatchEOL(&rest) ) {

    uint8_t buf[EROS_PAGE_SIZE];
    uint8_t *bufp;
    int i;

    vol_ReadLogPage(pVol, oid, buf);

    diag_printf("Log Page ");
    diag_printOid(oid);
    diag_printf(" cksum=0x%04x\n", ip_cksum((uint16_t*)buf, EROS_PAGE_SIZE));
    diag_printf("\n");

    bufp = buf;
    
    for (i = 0; i < 8; i++) {
      int j;
      diag_printf(" +0x%04x  ", i * 16);
      for (j = 0; j < 16; j++) {
	diag_printf("%02x ", *bufp);
	bufp++;
      }
      diag_printf("\n");
    }
    diag_printf("...\n");
    diag_printf("\n");
  }
  else if (parse_MatchStart(&rest, buf) &&
	   parse_MatchKeyword(&rest, "v") &&
	   parse_MatchEOL(&rest) ) {
    extern void PrintVolHdr(Volume *);
    PrintVolHdr(pVol);
  }
  else if (parse_MatchStart(&rest, buf) &&
	   parse_MatchKeyword(&rest, "c") &&
	   parse_MatchEOL(&rest) ) {
    extern void PrintCkptDir(Volume *);
    PrintCkptDir(pVol);
  }
  else if (parse_MatchStart(&rest, buf) &&
	   parse_MatchKeyword(&rest, "d") &&
	   parse_MatchEOL(&rest) ) {
    extern void PrintDivTable(Volume *);
    PrintDivTable(pVol);
  }
  else if (parse_MatchStart(&rest, buf) &&
	   parse_MatchKeyword(&rest, "kernel") &&
	   parse_MatchFileName(&rest, &fileName) &&
	   parse_MatchEOL(&rest) ) {
    int i;
    ExecImage *kernelImage = xi_create();

    if ( !xi_SetImage(kernelImage, fileName, 0, 0) ) {
      diag_error(1, "Couldn't load kernel image\n");
      return 1;
    }
      
    if (xi_NumRegions(kernelImage) != 1) {
      diag_error(1, "%s: kernel image improperly linked. Use '-n'!\n",
		 fileName);
      return 1;
    }

    for (i = 0; i < vol_MaxDiv(pVol); i++) {
      const Division *d = vol_GetDivision(pVol, i);
      if (d->type == dt_Kernel)
	vol_WriteKernelImage(pVol, i, kernelImage);
    }
    
    xi_destroy(kernelImage);
  }
  else if ( ( parse_MatchKeyword(&rest, "h") || parse_Match(&rest, "?") ) &&
	    parse_MatchEOL(&rest) ) {

    diag_printf("dbvol commands:\n");
    diag_printf("  n <oid>  - print out a node\n");
    diag_printf("  rn <oid> - print out a node as raw words\n");
    diag_printf("  cp <oid> - print out a cappage page\n");
    diag_printf("  p <oid>  - print out a page\n");
    diag_printf("  lp <loc> - print out a log page\n");
    diag_printf("  c        - print out the ckpt directory\n");
    diag_printf("  v        - print out the volume header\n");
    diag_printf("  d        - print out the division table\n");
    diag_printf("  r        - print out the rsrv table\n");
    diag_printf("  kernel   - install new kernel on existing volume\n");
    diag_printf("  h/?      - display this summary\n");
    diag_printf("\n");
  }
  else if (parse_MatchKeyword(&rest, "q") &&
	   parse_MatchEOL(&rest) )
    return 0;

  return 1;
}

int main(int argc, char *argv[])
{
  int c;
  extern int optind;
#if 0
  extern char *optarg;
#endif
  int opterr = 0;
  
  app_Init("dbvol");

  while ((c = getopt(argc, argv, "")) != -1) {
    switch(c) {
    default:
      opterr++;
    }
  }
  
      /* remaining arguments describe node and/or page space divisions */
  argc -= optind;
  argv += optind;
  
  if (argc != 1)
    opterr++;
  
  if (opterr)
    diag_fatal(1, "Usage: dbvol file\n");
  
  targname = *argv;
  
  pVol = vol_Open(targname, false, 0, 0, 0);
  if ( !pVol )
    diag_fatal(1, "Could not open \"%s\"\n", targname);
  
  while ( ProcessCmd() )
    ;

  vol_Close(pVol);
  free(pVol);
  
  app_Exit();
  exit(0);
}
