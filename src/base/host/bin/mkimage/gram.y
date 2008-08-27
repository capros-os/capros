%{
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <eros/target.h>
#include <erosimg/App.h>
#include <erosimg/ErosImage.h>
#include <erosimg/ExecImage.h>
#include <erosimg/Parse.h>
#include <erosimg/StrBuf.h>
#include <erosimg/DiskKey.h>
#include <eros/KeyConst.h>
#include <disk/DiskLSS.h>
#include <disk/DiskGPT.h>
#include <disk/Node.h>
#include <disk/Forwarder.h>
#include <eros/StdKeyType.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Forwarder.h>
#include "PtrMap.h"
#include "../../../lib/domain/include/domain/Runtime.h"

/* Made this a structure to avoid construction problems */
#include "ParseType.h"

bool showparse = false;

#define SHOWPARSE(s) if (showparse) diag_printf(s)
#define SHOWPARSE1(s,x) if (showparse) diag_printf(s,x)
     
extern void PrintDiskKey(KeyBits);

const char *CurArch = "Unknown Architecture";

const char *current_file = "<stdin>";
int current_line = 1;
extern FILE *yyin;

int num_errors = 0;  /* hold the number of syntax errors encountered. */

void ShowImageDirectory(const ErosImage *image);
void ShowImageThreadDirectory(const ErosImage *image);

bool
AddProgramSegment(ErosImage * image,
		  const char * fileName,
		  KeyBits * key,
                  uint32_t permMask, uint32_t permValue,
                  bool initOnly);

bool
GetProgramSymbolValue(const char *fileName,
		      const char *symName,
		      uint32_t *value);

bool
AddRawSegment(ErosImage *image,
	      const char *fileName,
	      KeyBits *key);

bool
AddZeroSegment(ErosImage *image,
	       KeyBits *key,
	       uint32_t nPages);

bool
AddEmptySegment(ErosImage *image,
	       KeyBits *key,
	       uint32_t nPages);

bool
GetMiscKeyType(const char *, uint32_t *ty);

bool
CheckSubsegOffset(const ErosImage * image, uint64_t offset,
                  KeyBits rootkey, KeyBits subSeg);

const char *strcons(const char *s1, const char * s2);

/* returns false on error */
bool QualifyKey(uint32_t, KeyBits in, KeyBits *out);
KeyBits NumberFromString(const char *);

#define ATTRIB_RO    capros_Memory_readOnly
#define ATTRIB_NC    capros_Memory_noCall
#define ATTRIB_WEAK  capros_Memory_weak
#define ATTRIB_OPAQUE capros_Memory_opaque

#define ATTRIB_SENDCAP capros_Forwarder_sendCap
#define ATTRIB_SENDWORD capros_Forwarder_sendWord

#define YYSTYPE ParseType

extern void yyerror(const char *);
extern int yylex();
/* extern int yylex (YYSTYPE *lvalp); */

ErosImage *image;

uint32_t DomRootRestriction[EROS_NODE_SIZE] = {
  RESTRICT_SCHED,		/* dr00: schedule slot */
  RESTRICT_START,		/* dr01: keeper slot */
  RESTRICT_SEGMODE,		/* dr02: address space */
  RESTRICT_KEYREGS,		/* dr03: key regs (future: capability space) */
  RESTRICT_IOSPACE,		/* dr04: i/o space */
  RESTRICT_SEGMODE,		/* dr05: reserved -- symbol space */
  RESTRICT_START,		/* dr06: brand */
  RESTRICT_NUMBER,		/* dr07: trap_code */
  RESTRICT_NUMBER,		/* dr08: pc, sp register */
  0,				/* dr09: architecture defined */
  0,				/* dr10: architecture defined */
  0,				/* dr11: architecture defined */
  0,				/* dr12: architecture defined */
  0,				/* dr13: architecture defined */
  0,				/* dr14: architecture defined */
  0,				/* dr15: architecture defined */
  0,				/* dr16: architecture defined */
  0,				/* dr17: architecture defined */
  0,				/* dr18: architecture defined */
  0,				/* dr19: architecture defined */
  0,				/* dr20: architecture defined */
  0,				/* dr21: architecture defined */
  0,				/* dr22: architecture defined */
  0,				/* dr23: architecture defined */
  0,				/* dr24: architecture defined */
  0,				/* dr25: architecture defined */
  0,				/* dr26: architecture defined */
  0,				/* dr27: architecture defined */
  0,				/* dr28: architecture defined */
  0,				/* dr29: architecture defined */
  0,				/* dr30: architecture defined */
  0,				/* dr31: architecture defined */
};

bool CheckRestriction(uint32_t restriction, KeyBits key);

%}

/* Following will not work until I hand-rewrite the lexer */
/* %pure_parser */

%token <NONE> DATA RW RO NC WEAK SENSE OPAQUE SENDCAP SENDWORD
%token <NONE> NODE PAGE PHYSPAGE FORWARDER GPT GUARD NEW HIDE PROGRAM SMALL
%token <NONE> CHAIN APPEND
%token PROCESS DOMAIN
/* %token <NONE> IMPORT */
%token <NONE> SEGMENT SEGTREE NULLKEY ZERO WITH PAGES RED EMPTY
%token <NONE> KW_LSS CONSTITUENTS
%token <NONE> ARCH PRINT SPACE REG KEEPER TARGET START BACKGROUND IOSPACE SYMTAB
%token PRIORITY SCHEDULE
%token <NONE> BRAND GENREG SLOT ROOT OREQ KEY SUBSEG AT CLONE COPY
%token <NONE> PC SP ALL SLOTS KEYS STRING LIMIT RUN IPL AS
%token <NONE> RANGE NUMBER SCHED MISC VOLSIZE DIRECTORY PRIME
%token <NONE> PHYSMEM
%token <NONE> SYMBOL CAPABILITY
%token <NONE> VOIDKEY
%token <is> NAME STRINGLIT HEX OCT BIN DEC WORD
%token <is> MISC_KEYNAME

%type <key> key segkey domain startkey
%type <key> qualified_key node forwarderkey GPTkey
%type <key> schedkey numberkey
/* Following are bare key names of appropriate type -- need to be
   distinguished to avoid shift/reduce errors: */
%type <key> segmode slot
%type <w> qualifier fwd_qualifier segtype blss keyData slotno
%type <w> number numeric_constant arith_expr add_expr mul_expr bit_expr
%type <w> offset priority
%type <oid> oid
%type <rd> arch_reg
%type <is> string_lit

%%

start:  /* empty */
        | start stmt
	;

stmt:   stmt ';' 
            { if (app_IsInteractive())
	        ShowImageDirectory(image);
	      SHOWPARSE("=== line -> stmt \\n\n"); }
        | error ';' {
	  yyerrok;
	}
	;

stmt:	/* IMPORT STRING {
	   SHOWPARSE("=== stmt -> IMPORT STRING\n");
	   ErosImage importImage;
	   importImage.ReadFromFile($2);

	   image.Import(importImage);
        }

       | */ HIDE NAME {
	   SHOWPARSE("=== stmt -> HIDE NAME\n");

	   if (ei_DelDirEnt(image, $2) == false) {
	     diag_printf("%s:%d: \"%s\" is not in the image directory\n",
			 current_file, current_line, $2);
	     num_errors++;
	     YYERROR;
	   }
        }

       | NAME '=' qualified_key {
	 /* no restrictions */
	   SHOWPARSE("=== stmt -> NAME = qualified_key\n");
	   ei_AssignDirEnt(image, $1, $3);
        }

       | slot '=' qualified_key {
	   SHOWPARSE("=== stmt -> slot = key\n");

	   if ( !CheckRestriction($<restriction>1, $3) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   ei_SetNodeSlot(image, $1, $<slot>1, $3);
        }

       | PRINT string_lit {
	   /* Really print quoted string... */
	   SHOWPARSE("=== stmt -> PRINT STRING\n");
	   diag_printf("%s\n", $2);
        }

       | PRINT DIRECTORY {
	   SHOWPARSE("=== stmt -> PRINT DIRECTORY\n");
	   ShowImageDirectory(image);
        }

       | PRINT key {
	   SHOWPARSE("=== stmt -> PRINT key\n");
      
	   PrintDiskKey($2);
	   diag_printf("\n");
        }

       | PRINT SEGMENT segkey {
	   SHOWPARSE("=== stmt -> PRINT SEGMENT segkey\n");
	   ei_PrintSegment(image, $3);
        }

       | PRINT PAGE key {
	   SHOWPARSE("=== stmt -> PRINT PAGE key\n");
	   if (keyBits_IsType(&$3, KKT_Page) == false) {
	     diag_printf("%s:%d: must be page key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   ei_PrintPage(image, $3);
        }

       | PRINT NODE key {
	   SHOWPARSE("=== stmt -> PRINT NODE key\n");

	   if (keyBits_IsNodeKeyType(&$3) == false) {
	     diag_printf("%s:%d: must be node key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   ei_PrintNode(image, $3);
        }

       | PRINT DOMAIN key {
	   SHOWPARSE("=== stmt -> PRINT DOMAIN key\n");
      
	   diag_warning("%s:%d: Obsolete syntax 'print domain', use 'print process'\n",
			 current_file, current_line);

	   if (keyBits_IsNodeKeyType(&$3) == false) {
	     diag_printf("%s:%d: must be node key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   ei_PrintDomain(image, $3);
        }

       | PRINT PROCESS key {
	   SHOWPARSE("=== stmt -> PRINT PROCESS key\n");
      
	   if (keyBits_IsNodeKeyType(&$3) == false) {
	     diag_printf("%s:%d: must be process or gate key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   ei_PrintDomain(image, $3);
        }

       | ARCH NAME {
	   uint32_t arch;

	   SHOWPARSE("=== stmt -> ARCH NAME\n");
	   arch = ExecArch_FromString($2);

	   if (arch == ExecArch_unknown) {
	     diag_printf("%s:%d: unknown architecture \"%s\".\n",
			 current_file, current_line, $2);
	     num_errors++;
	     YYERROR;
	   }
      
	   CurArch = $2;
        }

       | APPEND node key {
	  KeyBits chain = $2;
	  KeyBits key = $3;

	  SHOWPARSE("=== stmt -> APPEND node key\n");

          ei_AppendToChain(image, &chain, key);
       }

       | node SPACE '=' segkey {
	   SHOWPARSE("=== stmt -> domain SPACE = segkey\n");
	   if ( !CheckRestriction(DomRootRestriction[ProcAddrSpace], $4) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   ei_SetNodeSlot(image, $1, ProcAddrSpace, $4);
        }

       | node SYMTAB '=' key {
	   SHOWPARSE("=== stmt -> domain SYMTAB = key\n");
	   if ( !CheckRestriction(DomRootRestriction[ProcSymSpace], $4) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   ei_SetNodeSlot(image, $1, ProcSymSpace, $4);
        }

       | node IOSPACE '=' key {
	   SHOWPARSE("=== stmt -> domain IOSPACE = segkey\n");
	   if ( !CheckRestriction(DomRootRestriction[ProcIoSpace], $4) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   ei_SetNodeSlot(image, $1, ProcIoSpace, $4);
        }

       | node PC '=' arith_expr {
	   KeyBits key;

	   SHOWPARSE("=== stmt -> domain PC = arith_expr\n");
	   key = ei_GetNodeSlot(image, $1, ProcPCandSP);
	   if (keyBits_IsType(&key, KKT_Number) == false) {
	     diag_printf("%s:%d: Slot did not hold number key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   key.u.nk.value[0] = $4;
	   ei_SetNodeSlot(image, $1, ProcPCandSP, key);
        }

       | node SP '=' arith_expr {
	   KeyBits key;

	   SHOWPARSE("=== stmt -> domain SP = arith_expr\n");
	   key = ei_GetNodeSlot(image, $1, ProcPCandSP);
	   if (keyBits_IsType(&key, KKT_Number) == false) {
	     diag_printf("%s:%d: Slot did not hold number key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   key.u.nk.value[2] = $4;
	   ei_SetNodeSlot(image, $1, ProcPCandSP, key);
        }

       | node PC '=' numberkey {
	   KeyBits k;

	   SHOWPARSE("=== stmt -> domain PC = numberkey\n");
	   if ( !CheckRestriction(DomRootRestriction[ProcPCandSP], $4) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   k = ei_GetNodeSlot(image, $1, ProcPCandSP);
	   if (keyBits_IsType(&k, KKT_Number) == false) {
	     diag_printf("%s:%d: Slot did not hold number key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   k.u.nk.value[0] = $4.u.nk.value[0];
	   ei_SetNodeSlot(image, $1, ProcPCandSP, k);
        }

       | node PRIORITY '=' schedkey {
	   SHOWPARSE("=== stmt -> process PRIORITY = schedkey\n");
	  diag_warning("%s:%d: Obsolete syntax '<proc> priority', use '<proc> schedule'\n",
			 current_file, current_line);
	   if ( !CheckRestriction(DomRootRestriction[ProcSched], $4) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   ei_SetNodeSlot(image, $1, ProcSched, $4);
        }

       | node SCHEDULE '=' schedkey {
	   SHOWPARSE("=== stmt -> process SCHEDULE = schedkey\n");
	   if ( !CheckRestriction(DomRootRestriction[ProcSched], $4) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   ei_SetNodeSlot(image, $1, ProcSched, $4);
        }

       | node BRAND '=' startkey {
	   SHOWPARSE("=== stmt -> domain BRAND = startkey\n");
	   if ( !CheckRestriction(DomRootRestriction[ProcBrand], $4) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   ei_SetNodeSlot(image, $1, ProcBrand, $4);
        }

       | node DOMAIN KEEPER '=' startkey {
	   SHOWPARSE("=== stmt -> process DOMAIN KEEPER = startkey\n");
	  diag_warning("%s:%d: Obsolete syntax 'DOMAIN KEEPER', use 'new process'\n",
			 current_file, current_line);
	   if ( !CheckRestriction(DomRootRestriction[ProcKeeper], $5) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   ei_SetNodeSlot(image, $1, ProcKeeper, $5);
        }

       | node PROCESS KEEPER '=' startkey {
	   SHOWPARSE("=== stmt -> process PROCESS KEEPER = startkey\n");
	   if ( !CheckRestriction(DomRootRestriction[ProcKeeper], $5) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   ei_SetNodeSlot(image, $1, ProcKeeper, $5);
        }

       | forwarderkey TARGET '=' startkey {
	   SHOWPARSE("=== stmt -> forwarderkey TARGET = startkey\n");
	   if ( !CheckRestriction(RESTRICT_START, $4) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   ei_SetNodeSlot(image, $1, ForwarderTargetSlot, $4);
        }

       | GPTkey GPT KEEPER '=' startkey {
	   SHOWPARSE("=== stmt -> GPTkey GPT KEEPER = startkey\n");
	   if ( !CheckRestriction(RESTRICT_START, $5) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   ei_SetNodeSlot(image, $1, capros_GPT_keeperSlot, $5);
           ei_SetGPTFlags(image, $1, GPT_KEEPER);
        }

       | slot OREQ numberkey {
	   KeyBits key;

	   SHOWPARSE("=== stmt -> slot |= numberkey\n");
	   key = ei_GetNodeSlot(image, $1, $<slot>1);

	   if ( !CheckRestriction($<restriction>1, $3) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   if (keyBits_IsType(&key, KKT_Number) == false) {
	     diag_printf("%s:%d: Operator '|=' requires a number key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   key.u.nk.value[2] |= $3.u.nk.value[2];
	   key.u.nk.value[1] |= $3.u.nk.value[1];
	   key.u.nk.value[0] |= $3.u.nk.value[0];

	   ei_SetNodeSlot(image, $1, $<slot>1, $3);
        }

       | node KEY REG slotno '=' qualified_key {
	   KeyBits genKeys;

	   SHOWPARSE("=== stmt -> domain KEY REG keyreg_slotno = qualified_key\n");
	   if ( !CheckRestriction($4 ? 0 : RESTRICT_VOID, $6) ) {
	     diag_printf("%s:%d: key does not meet slot restriction\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   genKeys = ei_GetNodeSlot(image, $1, ProcGenKeys);
	   ei_SetNodeSlot(image, genKeys, $4, $6);
        }

       | node SLOTS '=' COPY node {
	 unsigned i;
	   KeyBits toKey = $1;
	   KeyBits fromKey = $5;

	   SHOWPARSE("=== stmt -> node SLOTS = COPY node\n");
      
	   for (i = 0; i < EROS_NODE_SIZE; i++) {
	     KeyBits tmp = ei_GetNodeSlot(image, fromKey, i);
	     ei_SetNodeSlot(image, toKey, i, tmp);
	   }
       }

       | segmode offset '=' WORD arith_expr {
	   uint32_t pageOffset;
	   KeyBits pageKey;

	   SHOWPARSE("=== stmt -> segment offset = WORD arith_expr\n");
	   keyBits_InitToVoid(&pageKey);
  
           if ($2 % 4) {
	     diag_printf("%s:%d: offset 0x%x not word-aligned\n",
			 current_file, current_line, $2);
	     num_errors++;
	     YYERROR;
           }

	   if (ei_GetPageInSegment(image, $1, $2, &pageKey) == false) {
	     pageKey = ei_AddZeroDataPage(image, false);
	     ei_AddSubsegToSegment(image, $1, $2, pageKey);
	   }
	  
	   pageOffset = $2 & EROS_PAGE_MASK;
           uint8_t * pageContent = ei_GetPageContentRef(image, &pageKey);
           *((uint32_t *) &pageContent[pageOffset]) = $5;
        }

/*
       | segmode SLOT slot '=' key {
	   SHOWPARSE("=== stmt -> node SLOT slot = key\n");
	   ei_SetNodeSlot(image, &$1, $3, $5);
        }
	*/
       | segmode ALL SLOTS '=' key {
	   unsigned i;

	   SHOWPARSE("=== stmt -> node ALL SLOTS = key\n");
	   for (i = 0; i < EROS_NODE_SIZE; i++) 
	     ei_SetNodeSlot(image, $1, i, $5);
        }

       | RUN domain {
	   SHOWPARSE("=== stmt -> RUN domain\n");

	   if (!ei_AddStartup(image, $<is>2, $2)) {
	     PrintDiskKey($2);
	     diag_printf("%s:%d: run stmt could not be applied\n",
			  current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
        }

       | IPL domain {
	   KeyBits key;

	   SHOWPARSE("=== stmt -> IPL domain\n");

	   keyBits_InitToVoid(&key);

	   if (ei_GetDirEnt(image, ":ipl:", &key)) {
	     diag_printf("%s:%d: redundant ipl statement ignored\n",
			  current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
	   else {
	     key = $2;
	     keyBits_SetType(&key, KKT_Process);
	     ei_AddDirEnt(image, ":ipl:", key);
	     ei_SetProcessState(image, key, RS_Running);
	   }
        }

       | domain REG arch_reg '=' HEX {
	   unsigned valueLen;

	   SHOWPARSE("=== stmt -> domain REG arch_reg = HEX\n");
	   valueLen = strlen($5 - 2);
	   if (valueLen > $3->len * 2) {
	     diag_printf("%s:%d: value \"%s\" exceeds "
			  "register size.\n",
			  current_file, current_line, $5);
	     num_errors++;
	     YYERROR;
	   }
	   RD_WriteValue($3, image, $1, $5);
        }
        ;

qualified_key: qualifier key {
          if ( !QualifyKey($1, $2, &$$) ) {
	    SHOWPARSE("--- !QualifyKey\n");
	     num_errors++;
	     YYERROR;
	  }
        }
       ;

key:   NULLKEY {
	  SHOWPARSE("=== key -> NULLKEY\n");
	  diag_printf("%s:%d: null key should often be replaced by "
		       "void key! \"%s\"\n",
		       current_file, current_line);
	  init_SmallNumberKey(&$$,0);
	}
       | VOIDKEY {
	  SHOWPARSE("=== key -> VOIDKEY\n");
	  init_MiscKey(&$$, KKT_Void, 0);
	}

       | VOLSIZE {
	  OID oid = 0x0;
	  SHOWPARSE("=== key -> VOLSIZE \n");
	  init_NodeKey(&$$, oid, 0);
	 }
       | NUMBER '(' string_lit ')' {
	  SHOWPARSE("=== key -> NUMBER ( STRING )\n");
	  $$ = NumberFromString($3);
	 }
       | NUMBER '(' arith_expr ')' {
	  SHOWPARSE("=== key -> NUMBER ( arith_expr )\n");
	  init_SmallNumberKey(&$$, $3);
	 }
       | NUMBER '(' arith_expr ',' arith_expr ',' arith_expr ')' {
	  SHOWPARSE("=== key -> NUMBER ( arith_expr , arith_expr , arith_expr )\n");
	  init_NumberKey(&$$, $3, $5, $7);
	 }
       | RANGE '(' oid ':' oid ')' {
	  SHOWPARSE("=== key -> RANGE ( oid : oid )\n");
	  init_RangeKey(&$$, $3, $5);
	 }
       | PRIME RANGE {
	  SHOWPARSE("=== key -> PRIME RANGE\n");
	  init_MiscKey(&$$, KKT_PrimeRange, 0);
	 }
       | PHYSMEM RANGE {
	  SHOWPARSE("=== key -> PHYSMEM RANGE\n");
	  init_MiscKey(&$$, KKT_PhysRange, 0);
	 }
       | SCHED '(' priority ')' {
	  SHOWPARSE("=== key -> SCHED ( priority )\n");
	  init_SchedKey(&$$, $3);
         }

       | MISC MISC_KEYNAME {
	  uint32_t miscType;

	  SHOWPARSE("=== key -> MISC NAME\n");
	  if (GetMiscKeyType($2, &miscType) == false) {
	    diag_printf("%s:%d: unknown misc key type \"%s\"\n",
			 current_file, current_line, $2);
	    num_errors++;
	    YYERROR;
	  }
	  
	  init_MiscKey(&$$, miscType, 0);
         }

       | MISC MISC_KEYNAME arith_expr{
	  uint32_t miscType;

	  SHOWPARSE("=== key -> MISC NAME arith_expr\n");
	  if (GetMiscKeyType($2, &miscType) == false) {
	    diag_printf("%s:%d: unknown misc key type \"%s\"\n",
			 current_file, current_line, $2);
	    num_errors++;
	    YYERROR;
	  }
	  
	  init_MiscKey(&$$, miscType, $3);
         }

       | MISC_KEYNAME {
	  uint32_t miscType;

	  SHOWPARSE("=== key -> MISC_KEYNAME\n");
	  if (GetMiscKeyType($1, &miscType) == false) {
	    diag_printf("%s:%d: unknown misc key type \"%s\"\n",
			 current_file, current_line, $1);
	    num_errors++;
	    YYERROR;
	  }
	  
	  init_MiscKey(&$$, miscType, 0);
         }
       | SEGTREE string_lit {
	  KeyBits key;

	  SHOWPARSE("=== key -> SEGTREE STRING\n");
	  keyBits_InitToVoid(&key);
	  if ( !AddRawSegment(image, $2, &key) ) {
	    num_errors++;
	    YYERROR;
	  }

	  $$ = key;
       }

       | SYMBOL string_lit NAME {
	   uint32_t new_pc;
	   KeyBits key;

	   SHOWPARSE("=== key -> STRING NAME\n");

	   keyBits_InitToVoid(&key);
	   
	   if ( GetProgramSymbolValue($2, $3, &new_pc) ) {
	     init_SmallNumberKey(&key, new_pc);
	   }
	   else {
	     diag_printf("%s:%d: Image \"%s\" did not have symbol \"%s\"\n",
			 $2, $3);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = key;
        }

       | PROGRAM segtype string_lit {
	   KeyBits key;

	   SHOWPARSE("=== key -> = PROGRAM segtype STRING\n");
	   
	   keyBits_InitToVoid(&key);

	   if (! AddProgramSegment(image, $3, &key, 0, 0, false) ) {
	     num_errors++;
	     YYERROR;
	   }

	   if ($2) {	// SEGMENT, not SEGTREE
	     if ( keyBits_IsType(&key, KKT_GPT) ) {	// not a page
               key.keyPerms |= capros_Memory_opaque;
	     }
	   }

	   $$ = key;
        }

       | PROGRAM RO SEGTREE string_lit {
	   KeyBits key;

	   SHOWPARSE("=== key -> = PROGRAM RO SEGTREE STRING\n");
	   
	   keyBits_InitToVoid(&key);

           /* Because we link with the -N option, the program header
           containing text is flagged ER_W. Therefore we use the ER_X
           flag to distinguish text from data.
           Unfortunately I can't get the linking to work right without -N. */
	   if (! AddProgramSegment(image, $4, &key, ER_X, ER_X, false) ) {
	     num_errors++;
	     YYERROR;
	   }

	   $$ = key;
        }

       | PROGRAM RW SEGTREE string_lit {
	   KeyBits key;

	   SHOWPARSE("=== key -> = PROGRAM RW SEGTREE STRING\n");
	   
	   keyBits_InitToVoid(&key);

	   if (! AddProgramSegment(image, $4, &key, ER_X, 0, false) ) {
	     num_errors++;
	     YYERROR;
	   }

	   $$ = key;
        }

       | PROGRAM DATA SEGTREE string_lit {
	   KeyBits key;

	   SHOWPARSE("=== key -> = PROGRAM DATA SEGTREE STRING\n");
	   
	   keyBits_InitToVoid(&key);

	   if (! AddProgramSegment(image, $4, &key, ER_X, 0, true) ) {
	     num_errors++;
	     YYERROR;
	   }

	   $$ = key;
        }

       | SMALL PROGRAM string_lit {
	   KeyBits key;

	   SHOWPARSE("=== key -> SMALL PROGRAM string_lit\n");

	   keyBits_InitToVoid(&key);

	   if (! AddProgramSegment(image, $3, &key, 0, 0, false) ) {
	     num_errors++;
	     YYERROR;
	   }

	   if ( keyBits_IsType(&key, KKT_Page) ) {
	     KeyBits segKey = ei_AddNode(image, false);
	     keyBits_SetType(&segKey, KKT_GPT);
             keyBits_SetL2g(&segKey, 64);
             ei_SetBlss(image, segKey, EROS_PAGE_BLSS + 1);

	     ei_SetNodeSlot(image, segKey, 0, key);

	     key = segKey;
	   }
           else {
	     if (ei_GetAnyBlss(image, key) > (EROS_PAGE_BLSS + 1)) {
	       diag_printf("%s:%d: binary image too large for small program\n",
	  		 current_file, current_line);
	       num_errors++;
	       YYERROR;
	     }
           }

	   $$ = key;
        }

       | ZERO SEGTREE WITH arith_expr PAGES {
	  KeyBits key;

	  SHOWPARSE("=== key -> ZERO SEGTREE WITH arith_expr PAGES\n");
	  keyBits_InitToVoid(&key);

	  if ( !AddZeroSegment(image, &key, $4) ) {
	    num_errors++;
	    YYERROR;
	  }
	  
	  $$ = key;
       }

       | EMPTY GPT WITH arith_expr PAGES {
	  KeyBits key;

	  SHOWPARSE("=== key -> EMPTY GPT WITH arith_expr PAGES\n");

	  if ( !AddEmptySegment(image, &key, $4) ) {
	    num_errors++;
	    YYERROR;
	  }
	  
	  $$ = key;
       }

       | NEW qualifier NODE {
	  KeyBits key;

	  SHOWPARSE("=== key -> NEW qualifier NODE\n");
	  key = ei_AddNode(image, false);

	  SHOWPARSE("Reduce key -> NEW qualifier NODE\n");
	  if ( !QualifyKey($2, key, &key) ) {
	    num_errors++;
	    YYERROR;
	  }

	  $$ = key;
        }

       | NEW FORWARDER {
	  KeyBits key;
	  KeyBits fmtKey;

	  SHOWPARSE("=== key -> NEW FORWARDER\n");
	  key = ei_AddNode(image, false);

	  init_SmallNumberKey(&fmtKey, 0);
	  ei_SetNodeSlot(image, key, ForwarderDataSlot, fmtKey);

	  keyBits_SetType(&key, KKT_Forwarder);

	  $$ = key;
        }

       | NEW GPT WITH blss {
	  KeyBits key;

	  SHOWPARSE("=== key -> NEW GPT WITH blss\n");
	  key = ei_AddNode(image, false);
          keyBits_SetType(&key, KKT_GPT);
          keyBits_SetL2g(&key, 64);

          if (! ei_SetBlss(image, key, $4)) {
	    diag_printf("%s:%d: invalid lss value\n",
	                current_file, current_line);
	    num_errors++;
	    YYERROR;
          }

	  $$ = key;
        }

       | NEW qualifier CHAIN {
	  KeyBits key;
	  SHOWPARSE("=== key -> NEW qualifier NODE\n");
	  key = ei_AddNode(image, false);

	  SHOWPARSE("Reduce key -> NEW qualifier NODE\n");
	  if ( !QualifyKey($2, key, &key) ) {
	    num_errors++;
	    YYERROR;
	  }

	  $$ = key;
        }

       | PHYSPAGE '(' arith_expr ')' {
          OID oid = (OID)($3) / (EROS_PAGE_SIZE / EROS_OBJECTS_PER_FRAME)
                    + OID_RESERVED_PHYSRANGE;

	  SHOWPARSE("=== key -> PHYSPAGE ( arith_expr )\n");
	  init_DataPageKey(&$$, oid, false);
        }

       | PHYSPAGE RO '(' arith_expr ')' {
          OID oid = $4 / (EROS_PAGE_SIZE / EROS_OBJECTS_PER_FRAME)
                    + OID_RESERVED_PHYSRANGE;

	  SHOWPARSE("=== key -> RO PHYSPAGE ( arith_expr )\n");
	  init_DataPageKey(&$$, oid, true);
        }

       | NEW qualifier PAGE {
	  KeyBits key;

	  SHOWPARSE("=== key -> NEW qualifier PAGE\n");
	  key = ei_AddZeroDataPage(image, false);

	  if ( !QualifyKey($2, key, &key) ) {
	    num_errors++;
	    YYERROR;
	  }

	  $$ = key;
        }

       | NAME {
	  KeyBits key;

	  SHOWPARSE("=== key -> NAME\n");
	  keyBits_InitToVoid(&key);
      
	  if (ei_GetDirEnt(image, $1, &key) == false) {
	    diag_printf("%s:%d: unknown object \"%s\"\n",
			 current_file, current_line, $1);
	    num_errors++;
	    YYERROR;
	  }
      
	  $$ = key;
        }

       | CLONE node {
	  KeyBits nodeKey;
	  KeyBits key;
	  unsigned i;

	  SHOWPARSE("=== key -> CLONE key\n");
	  nodeKey = $2;
      
	  key = ei_AddNode(image, false);
          // Copy nodeData:
          ei_SetNodeData(image, key, ei_GetNodeData(image, nodeKey));
	  for (i = 0; i < EROS_NODE_SIZE; i++) {
	    KeyBits tmp = ei_GetNodeSlot(image, nodeKey, i);
	    ei_SetNodeSlot(image, key, i, tmp);
	  }
	  
	  /* Patch the resulting key so that the permissions and type
	   * match those of the incoming key: */
	  key.keyType = nodeKey.keyType;
	  key.keyFlags = nodeKey.keyFlags;
	  key.keyPerms = nodeKey.keyPerms;
	  key.keyData = nodeKey.keyData;

	  $$ = key;
        }

       | key WITH blss {
	  KeyBits key;

	  SHOWPARSE("=== key -> key NAME blss\n");
      
	  key = $1;

	  if (keyBits_IsSegModeType(&key) == false) {
	    diag_printf("%s:%d: can only set BLSS on segmode keys\n",
			 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

	  ei_SetBlss(image, key, $3);

	  $$ = key;
        }

       | key WITH CONSTITUENTS {
	  KeyBits rootKey;
	  KeyBits keyRegsKey;
	  KeyBits constituents;
	  SHOWPARSE("=== key -> key WITH CONSTITUENTS\n");
      
	  rootKey = $1;

	  if (keyBits_IsType(&rootKey, KKT_Process) == false) {
	    diag_printf("%s:%d: can only add constituents to a process\n",
			 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

	  keyRegsKey = ei_GetNodeSlot(image, rootKey, ProcGenKeys);
	  constituents = ei_AddNode(image, false);
          /* For node_extended_copy:
	  keyBits_SetBlss(&constituents, EROS_PAGE_BLSS);
          */
	  keyBits_SetReadOnly(&constituents);
	  ei_SetNodeSlot(image, keyRegsKey, KR_CONSTIT, constituents);

	  $$ = rootKey;
        }

       | key AS qualifier GPT KEY {
	  KeyBits key;

	  SHOWPARSE("=== key -> key AS qualifier GPT KEY\n");
      
	  key = $1;

          if (! keyBits_IsType(&$1, KKT_GPT)) {
	     diag_printf("%s:%d: must be GPT key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	  if ( !QualifyKey($3, key, &key) ) {
	    num_errors++;
	    YYERROR;
	  }

	  $$ = key;
        }

       | key WITH GUARD offset {
	  KeyBits key;
	  uint64_t offset;

	  SHOWPARSE("=== key -> key WITH GUARD offset\n");
      
	  key = $1;

          if (! keyBits_IsSegModeType(&$1)) {
	     diag_printf("%s:%d: must be GPT or page key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	  offset = $4;
          struct GuardData gd;
          if (! key_CalcGuard(offset, &gd)) {
	    diag_printf("%s:%d: Invalid guard\n",
		 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }
          key_SetGuardData(&key, &gd);

	  $$ = key;
        }

       | key AS OPAQUE FORWARDER KEY {
	  KeyBits key;

	  SHOWPARSE("=== key -> key AS OPAQUE FORWARDER KEY\n");
      
	  key = $1;

	  if (! keyBits_IsType(&key, KKT_Forwarder)) {
	    diag_printf("%s:%d: must be forwarder key\n",
			 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

          key.keyData |= capros_Forwarder_opaque;

	  $$ = key;
        }

       | key AS fwd_qualifier OPAQUE FORWARDER KEY {
	  KeyBits key;

	  SHOWPARSE("=== key -> key AS fwd_qualifier OPAQUE FORWARDER KEY\n");
      
	  key = $1;

	  if (! keyBits_IsType(&key, KKT_Forwarder)) {
	    diag_printf("%s:%d: must be forwarder key\n",
			 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

          key.keyData |= capros_Forwarder_opaque;
          if ($3 & ATTRIB_SENDCAP) {
            key.keyData |= capros_Forwarder_sendCap;
          }
          if ($3 & ATTRIB_SENDWORD) {
            key.keyData |= capros_Forwarder_sendWord;
          }

	  $$ = key;
        }

       | key AS OPAQUE GPT KEY {
	  KeyBits key;

	  SHOWPARSE("=== key -> key AS OPAQUE GPT KEY\n");
      
	  key = $1;

	  if (! keyBits_IsType(&key, KKT_GPT)) {
	    diag_printf("%s:%d: must be GPT key key\n",
			 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

          key.keyPerms |= capros_Memory_opaque;

	  $$ = key;
        }

       | key AS qualifier NODE KEY {
	  KeyBits key;

	  SHOWPARSE("=== key -> key AS NODE KEY\n");
      
	  key = $1;

	  if (! keyBits_IsType(&key, KKT_Node)) {
	    diag_printf("%s:%d: cannot retype to node key\n",
			 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

	  if ( !QualifyKey($3, key, &key) ) {
	    num_errors++;
	    YYERROR;
	  }

	  $$ = key;
        }

       | key WITH qualifier PAGE AT offset {
	   KeyBits key;
	   KeyBits pageKey;
	   uint64_t offset;

	   SHOWPARSE("=== key -> qualifier NAME with page_qualifier PAGE AT offset\n");
	   key = $1;
      
	   /*
	   if (image.GetDirEnt($1, key) == false) {
	     diag_printf("%s:%d: unknown object \"%s\"\n",
			 current_file, current_line, $1);
	    num_errors++;
	     YYERROR;
	   }
	   */
	   
	   if (keyBits_IsSegModeType(&key) == false) {
	     diag_printf("%s:%d: can only add pages to segments!\n",
			  current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   pageKey =
	     ei_AddZeroDataPage(image, $3 & (ATTRIB_RO) ? true : false);
      
	   offset = $6;
           if (! CheckSubsegOffset(image, offset, key, pageKey)) {
	     num_errors++;
	     YYERROR;
           }

	   key = ei_AddSubsegToSegment(image, key, offset, pageKey);
      
	   $$ = key;
        }

       | key WITH qualifier SUBSEG segkey AT offset {
	  KeyBits key;
	  KeyBits subSeg;
	  uint64_t offset;

	  SHOWPARSE("=== key -> qualifier NAME with qualifier SUBSEG AT offset\n");
	  key = $1;
      
	  /*
	  if (image.GetDirEnt($1, key) == false) {
	    diag_printf("%s:%d: unknown object \"%s\"\n",
			 current_file, current_line, $1);
	    num_errors++;
	    YYERROR;
	  }
	  */

	  if (keyBits_IsSegModeType(&key) == false) {
	    diag_printf("%s:%d: can only add subsegments to segments!\n",
			 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

	  subSeg = $5;

	  if (keyBits_IsSegModeType(&subSeg) == false) {
	    diag_printf("%s:%d: qualifiers only permitted on"
			 " segmode keys\n",
			 current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

	  offset = $7;
           if (! CheckSubsegOffset(image, offset, key, subSeg)) {
	     num_errors++;
	     YYERROR;
           }

	  if ( !QualifyKey($3, subSeg, &subSeg) ) {
	    num_errors++;
	    YYERROR;
	  }
	   
	  key = ei_AddSubsegToSegment(image, key, offset, subSeg);

	  $$ = key;
        }

       | NEW DOMAIN {
	  KeyBits key;

	  SHOWPARSE("=== key -> NEW DOMAIN\n");
	  diag_warning("%s:%d: Obsolete syntax 'new domain', use 'new process'\n",
			 current_file, current_line);
	  key = ei_AddProcess(image);
      
	  RD_InitProcess(image, key, CurArch);
      
	  keyBits_SetType(&key, KKT_Process);

	  $$ = key;
        }

       | NEW PROCESS {
	  KeyBits key;

	  SHOWPARSE("=== key -> NEW PROCESS\n");
	  key = ei_AddProcess(image);
      
	  RD_InitProcess(image, key, CurArch);
      
	  keyBits_SetType(&key, KKT_Process);

	  $$ = key;
        }

       | START domain keyData {
	  KeyBits key;

	  SHOWPARSE("=== key -> START domain keyData\n");
	  key = $2;
	  keyBits_SetType(&key, KKT_Start);
	  key.keyFlags = 0;
	  key.keyPerms = 0;
	  key.keyData = $3;

	  $$ = key;
        }
       | slot {
	  SHOWPARSE("=== key -> slot\n");
	  $$ = ei_GetNodeSlot(image, $1, $<slot>1);
        }
       ;

domain:  NAME {
	   KeyBits key;

	   SHOWPARSE("=== domain -> NAME\n");
	   keyBits_InitToVoid(&key);

	   if (ei_GetDirEnt(image, $1, &key) == false) {
	     diag_printf("%s:%d: unknown object \"%s\"\n",
			 current_file, current_line, $1);
	     num_errors++;
	     YYERROR;
	   }

           if (keyBits_IsType(&key, KKT_Process) == false &&
	       keyBits_IsType(&key, KKT_Start) == false &&
	       keyBits_IsType(&key, KKT_Resume) == false) {
	     diag_printf("%s:%d: must be domain name\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $<is>$ = $1;
	   $$ = key;
        }
        ;

node:  NAME {
	   KeyBits key;

	   SHOWPARSE("=== node -> NAME\n");
	   keyBits_InitToVoid(&key);
      
	   if (ei_GetDirEnt(image, $1, &key) == false) {
	     diag_printf("%s:%d: unknown object \"%s\"\n",
			 current_file, current_line, $1);
	     num_errors++;
	     YYERROR;
	   }

           if (keyBits_IsNodeKeyType(&key) == false) {
	     diag_printf("%s:%d: must be node key type\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = key;
        }
       | slot {
	   KeyBits key;

	   SHOWPARSE("=== node -> slot\n");
	   key = ei_GetNodeSlot(image, $1, $<slot>1);
	   
           if (keyBits_IsNodeKeyType(&key) == false) {
	     diag_printf("%s:%d: must be node key type\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = key;
        }
        ;

segmode:  NAME {
	   KeyBits key;

	   SHOWPARSE("=== segmode -> NAME\n");
	   keyBits_InitToVoid(&key);
      
	   if (ei_GetDirEnt(image, $1, &key) == false) {
	     diag_printf("%s:%d: unknown object \"%s\"\n",
			 current_file, current_line, $1);
	     num_errors++;
	     YYERROR;
	   }

	   if ( keyBits_IsSegKeyType(&key) ) {
	     $$ = key;
	   }
	   else {
	     diag_printf("%s:%d: must be segmode key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }
        }
        ;

/*
domainkey:  key {
	  SHOWPARSE("=== domainkey -> key\n");
	   if ($1.IsType(KKT_Process) == false) {
	     diag_printf("%s:%d: must be domain key\n",
			 current_file, current_line);
	    num_errors++;
	     YYERROR;
	   }

	   $$ = $1;
        }
        ;
	*/

startkey:  key {
	  SHOWPARSE("=== startkey -> key\n");
      
          if (keyBits_IsType(&$1, KKT_Start) == false) {
	     diag_printf("%s:%d: \"%s\" must be start key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = $1;
        }
        ;

segkey:  key {
	  SHOWPARSE("=== segkey -> key\n");
          if (keyBits_IsSegModeType(&$1) == false
	       && keyBits_IsVoidKey(&$1) == false
	       ) {
	     diag_printf("%s:%d: must be segment or "
			 "segtree key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = $1;
        }
        ;

forwarderkey:  key {
	  SHOWPARSE("=== forwarderkey -> key\n");
          if (! keyBits_IsType(&$1, KKT_Forwarder)) {
	     diag_printf("%s:%d: must be forwarder key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = $1;
        }
        ;

GPTkey:  key {
	  SHOWPARSE("=== GPTkey -> key\n");
          if (! keyBits_IsType(&$1, KKT_GPT)) {
	     diag_printf("%s:%d: must be GPT key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = $1;
        }
        ;

schedkey:  key {
	  SHOWPARSE("=== schedkey -> key\n");
          if (keyBits_IsType(&$1, KKT_Sched) == false) {
	     diag_printf("%s:%d: must be schedule key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = $1;
        }
        ;

numberkey:  key {
	  SHOWPARSE("=== numberkey -> key\n");
          if (keyBits_IsType(&$1, KKT_Number) == false) {
	     diag_printf("%s:%d: must be number key\n",
			 current_file, current_line);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = $1;
        }
        ;

arch_reg:  NAME {
           RegDescrip *rd;
	   SHOWPARSE("=== arch_reg -> NAME\n");
            /* See if it's really an architecture name: */
           rd = RD_Lookup(CurArch, $1);

	   if (rd == 0) {
	     diag_printf("%s:%d: \"%s\" is not a valid "
			 "register name\n",
			 current_file, current_line, $1);
	     num_errors++;
	     YYERROR;
	   }

	   $$ = rd;
        }
        ;

segtype:  SEGMENT {
	   SHOWPARSE("=== segtype -> SEGMENT\n");
          $$ = 1;
	   }
        | SEGTREE {
	   SHOWPARSE("=== segtype -> SEGTREE\n");
	  $$ = 0;
	}
        ;

fwd_qualifier:  SENDCAP {
	  SHOWPARSE("=== fwd_qualifier -> SENDCAP\n");
          $$ = ATTRIB_SENDCAP;
	   }
        |  SENDWORD {
	  SHOWPARSE("=== fwd_qualifier -> SENDWORD\n");
	  $$ =  ATTRIB_SENDWORD;
	   }
        |  SENDCAP SENDWORD {
	  SHOWPARSE("=== fwd_qualifier -> SENDCAP SENDWORD\n");
	  $$ =  ATTRIB_SENDCAP | ATTRIB_SENDWORD;
	   }
        |  SENDWORD SENDCAP {
	  SHOWPARSE("=== fwd_qualifier -> SENDWORD SENDCAP\n");
	  $$ =  ATTRIB_SENDCAP | ATTRIB_SENDWORD;
	   }
        ;

qualifier:  RO {
	  SHOWPARSE("=== qualifier -> RO\n");
          $$ = ATTRIB_RO;
	   }
        |  RO NC {
	  SHOWPARSE("=== qualifier -> RO NC\n");
	  $$ =  ATTRIB_RO | ATTRIB_NC;
	   }
        |  RO NC WEAK {
	  SHOWPARSE("=== qualifier -> RO NC\n");
	  $$ =  ATTRIB_RO | ATTRIB_NC | ATTRIB_WEAK;
	   }
        |  RO WEAK {
	  SHOWPARSE("=== qualifier -> RO NC\n");
	  $$ =  ATTRIB_RO | ATTRIB_WEAK;
	   }
        |  RO WEAK NC {
	  SHOWPARSE("=== qualifier -> RO NC\n");
	  $$ =  ATTRIB_RO | ATTRIB_NC | ATTRIB_WEAK;
	   }
        |  NC {
	  SHOWPARSE("=== qualifier -> NC\n");
	  $$ = ATTRIB_NC;
	   }
        |  NC RO {
	  SHOWPARSE("=== qualifier -> NC RO\n");
	  $$ =  ATTRIB_RO | ATTRIB_NC;
	   }
        |  NC RO WEAK {
	  SHOWPARSE("=== qualifier -> NC RO\n");
	  $$ =  ATTRIB_RO | ATTRIB_NC | ATTRIB_WEAK;
	   }
        |  NC WEAK {
	  SHOWPARSE("=== qualifier -> NC RO\n");
	  $$ =  ATTRIB_RO | ATTRIB_NC;
	   }
        |  NC WEAK RO {
	  SHOWPARSE("=== qualifier -> NC RO\n");
	  $$ =  ATTRIB_RO | ATTRIB_NC | ATTRIB_WEAK;
	   }
        |  WEAK {
	  SHOWPARSE("=== qualifier -> NC\n");
	  $$ = ATTRIB_WEAK;
	   }
        |  WEAK NC {
	  SHOWPARSE("=== qualifier -> NC\n");
	  $$ = ATTRIB_WEAK | ATTRIB_NC;
	   }
        |  WEAK NC RO {
	  SHOWPARSE("=== qualifier -> NC\n");
	  $$ = ATTRIB_WEAK | ATTRIB_NC | ATTRIB_RO;
	   }
        |  WEAK RO {
	  SHOWPARSE("=== qualifier -> NC\n");
	  $$ = ATTRIB_WEAK | ATTRIB_RO;
	   }
        |  WEAK RO NC {
	  SHOWPARSE("=== qualifier -> NC\n");
	  $$ = ATTRIB_WEAK | ATTRIB_NC | ATTRIB_RO;
	   }
        |  SENSE {
	  SHOWPARSE("=== qualifier -> SENSE\n");
	  $$ = ATTRIB_WEAK | ATTRIB_NC | ATTRIB_RO;
	   }
        | /* nothing */ {
	  SHOWPARSE("=== qualifier -> <nothing>\n");
	  $$ = 0;
	   }
        ;

/* OID can only be hex to simplify conversion */
oid:   HEX {
	  int len;
	  char digits[19];
	  uint32_t lo;
	  uint32_t hi = 0;
	  uint64_t oid;
	 
          SHOWPARSE("=== oid -> HEX\n");

          len = strlen($1);
	  if (len > 18) {
	    diag_printf("%s:%d: oid value too large\n",
			current_file, current_line);
	    num_errors++;
	    YYERROR;
	  }

	  strcpy(digits, $1);

	  if ( len > 10 ) {
	    /* has significant upper digits... */
	    lo = strtoul(digits + (len - 8), 0, 16);
	    digits[len - 8] = 0;
	    hi = strtoul(digits, 0, 16);
	  }
	  else {
	    /* has significant upper digits... */
	    lo = strtoul(digits, 0, 16);
	  }

	  oid = hi;
	  oid <<= 32;
	  oid |= (uint64_t) lo;

	  $$ = oid;
        }
        ;

blss:  KW_LSS arith_expr {
         SHOWPARSE("=== lss -> arith_expr\n");
	 if ($2 > (MAX_BLSS - EROS_PAGE_BLSS)) {
	   diag_printf("%s:%d: lss value too large\n",
		       current_file, current_line);
	   num_errors++;
	   YYERROR;
	 }

	 if ($2 == 0) {
	   diag_printf("%s:%d: lss value must be > 0\n",
		       current_file, current_line);
	   num_errors++;
	   YYERROR;
	 }
	 $$ = $2 + EROS_PAGE_BLSS;
       }
       ;

priority:  arith_expr {
          SHOWPARSE("=== priority -> arith_expr\n");
	 if ($1 > pr_High) {
	   diag_printf("%s:%d: priority value too large\n",
		       current_file, current_line);
	   num_errors++;
	   YYERROR;
	 }
       }
       ;

slot: NAME '[' slotno ']' {
	 KeyBits key;
	 uint32_t restriction = 0;

         SHOWPARSE("=== slot -> NAME [ slotno ]\n");

	 keyBits_InitToVoid(&key);

	 if (ei_GetDirEnt(image, $1, &key) == false) {
	   diag_printf("%s:%d: unknown object \"%s\"\n",
			current_file, current_line, $1);
	   num_errors++;
	   YYERROR;
	 }


	 $$ = key;
	 $<slot>$ = $3;

	 if (keyBits_IsNodeKeyType(&key) == false) {
	   diag_printf("%s:%d: must be node key type\n",
			current_file, current_line);
	   num_errors++;
	   YYERROR;
	 }

	 if (keyBits_IsType(&key, KKT_Process) || keyBits_IsType(&key, KKT_Start) ||
	     keyBits_IsType(&key, KKT_Resume)) {
	   restriction |= DomRootRestriction[$3];
	 }

	 $<restriction>$ = restriction;
         SHOWPARSE1("=== +++ restriction = 0x%x\n", restriction);
       }
       | slot '[' slotno ']'  {
	 KeyBits key;
	 uint32_t restriction = 0;
	 
	 if ($<restriction>1 & (RESTRICT_VOID|RESTRICT_NUMBER|RESTRICT_SCHED)) {
	   diag_printf("%s:%d: cannot indirect through number/void key\n",
			current_file, current_line);
	   num_errors++;
	   YYERROR;
	 }

	 if ($<restriction>1 & RESTRICT_GENREGS)
	   restriction |= RESTRICT_NUMBER;

	 if (($<restriction>1 & RESTRICT_KEYREGS) && $3 == 0)
	   restriction |= RESTRICT_VOID;

	 key = ei_GetNodeSlot(image, $1, $<slot>1);

	 $<slot>$ = $3;
	 $$ = key;
	 
	 if ( !keyBits_IsNodeKeyType(&key) ) {
	   diag_printf("%s:%d: LHS of '[slot]' must be segmode key\n"
			"    You may need to say \"new [small] process with constituents\"\n"
			"                                             ^^^^^^^^^^^^^^^^^\n",
			current_file, current_line);
	   num_errors++;
	   YYERROR;
	 }

	 if (keyBits_IsType(&key, KKT_Process) || keyBits_IsType(&key, KKT_Start) ||
	     keyBits_IsType(&key, KKT_Resume)) {
	   restriction |= DomRootRestriction[$3];
	 }

	 $<restriction>$ = restriction;
         SHOWPARSE1("=== +++ restriction = 0x%x\n", restriction);
        }
	;

slotno:  arith_expr {
          SHOWPARSE("=== slotno -> arith_expr\n");
	 if ($1 >= EROS_NODE_SIZE) {
	   diag_printf("%s:%d: slot index too large\n",
		       current_file, current_line);
	   num_errors++;
	   YYERROR;
	 }
       }
       ;

offset:  arith_expr {
          SHOWPARSE("=== offset -> arith_expr\n");

  /* This is WRONG -- the offset should be a 64 bit quantity */
         $$ = $1;
       }
       ;

keyData:  arith_expr {
          SHOWPARSE("=== keyData -> arith_expr\n");
	 if ($1 > 0xffffu) {
	   diag_printf("%s:%d: key data value too large\n",
		       current_file, current_line);
	   num_errors++;
	   YYERROR;
	 }
       }
       ;

arith_expr: bit_expr {
  /* FIX: Not sure that the operator precedence is correct
   * here. Where should bit ops be in the operator precedence scale? */
         SHOWPARSE("=== arith_expr -> bit_expr\n");
         $$ = $1;
       }
       ;

bit_expr: add_expr '|' add_expr {
         SHOWPARSE("=== bit_expr -> add_expr '|' add_expr\n");
         $$ = $1 | $3;
       }
     | add_expr '^' add_expr {
         SHOWPARSE("=== bit_expr -> add_expr '^' add_expr\n");
         $$ = $1 ^ $3;
       }
     | add_expr '&' add_expr {
         SHOWPARSE("=== bit_expr -> add_expr '&' add_expr\n");
         $$ = $1 & $3;
       }
     | add_expr {
         SHOWPARSE("=== bit_expr -> add_expr\n");
         $$ = $1;
       }
       ;

add_expr: mul_expr '+' mul_expr {
         SHOWPARSE("=== add_expr -> mul_expr '+' mul_expr\n");
         $$ = $1 + $3;
       }
     | mul_expr '-' mul_expr {
         SHOWPARSE("=== add_expr -> mul_expr '-' mul_expr\n");
         $$ = $1 - $3;
       }
     | mul_expr {
         SHOWPARSE("=== add_expr -> mul_expr\n");
         $$ = $1;
       }
       ;

mul_expr: number {
         SHOWPARSE("=== add_expr -> number\n");
         $$ = $1;
       }
     | mul_expr '*' number {
         SHOWPARSE("=== mul_expr -> mul_expr '*' number\n");
         $$ = $1 * $3;
       }
     | mul_expr '%' number {
         SHOWPARSE("=== mul_expr -> mul_expr '%' number\n");
         $$ = $1 * $3;
       }
     | mul_expr '/' number {
         SHOWPARSE("=== mul_expr -> mul_expr '/' number\n");
         $$ = $1 / $3;
       }
       ;

number: numeric_constant {
         SHOWPARSE("=== number -> numeric_constant\n");
         $$ = $1;
       }
     | '(' arith_expr ')' {
         SHOWPARSE("=== number -> '(' arith_expr ')'\n");
         $$ = $2;
       }
     | '-' number {
         SHOWPARSE("=== number -> '-' number\n");
         $$ = - $2;
       }
     | '~' number {
         SHOWPARSE("=== number -> '~' number\n");
         $$ = ~ $2;
       }
       ;

numeric_constant:  HEX {
         unsigned long value;
         SHOWPARSE("=== number -> HEX\n");
         value = strtoul($1, 0, 0);
	 $$ = value;
       }
     | DEC {
         unsigned long value;
         SHOWPARSE("=== number -> DEC\n");
         value = strtoul($1, 0, 0);
	 $$ = value;
       }

     | OCT {
         unsigned long value;
         SHOWPARSE("=== number -> OCT\n");
         value = strtoul($1, 0, 0);
	 $$ = value;
       }

     | BIN {
         unsigned long value;
         SHOWPARSE("=== number -> BIN\n");
	 value = strtoul($1+2, 0, 2);
	 $$ = value;
       }
     ;

string_lit: STRINGLIT {
         SHOWPARSE("=== string_lit -> STRINGLIT\n");
         $$ = $1;
       }
     | string_lit STRINGLIT {
         SHOWPARSE("=== string_lit => string_lit STRINGLIT\n");
	 $$ = strcons($1,$2);
       }
     ;
%%

const char *
strcons(const char *s1, const char *s2)
{
  size_t len = strlen(s1) + strlen(s2);
  char *s = (char *) malloc(len+1);
  strcpy(s, s1);
  strcat(s, s2);
  s[len] = 0;

  return internWithLength(s, len);
}

int
main(int argc, char *argv[])
{
  int c;
  extern int optind;
#if 0
  extern char *optarg;
#endif
  extern int yy_flex_debug;
  
  int opterr = 0;
  const char *output;
  const char *architecture;
  bool verbose = false;
  bool use_std_inc_path = true;
  bool use_std_def_list = true;
  StrBuf *cpp_cmd;
  StrBuf *cpp_cmd_args;
  StrBuf *stdinc;
  StrBuf *stddef;
  uint32_t arch;

  app_Init("mkimage");

  yy_flex_debug = 0;		/* until proven otherwise */

  cpp_cmd = strbuf_create();
  cpp_cmd_args = strbuf_create();
  stdinc = strbuf_create();
  stddef = strbuf_create();

  strbuf_append(cpp_cmd, "/lib/cpp ");
    
  /* Set up standard EROS include search path: */
  strbuf_append(stdinc, "-I");
  strbuf_append(stdinc, app_BuildPath("/eros/include"));
  strbuf_append_char(stdinc, ' ');

  strbuf_append_char(stddef, ' ');

  while ((c = getopt(argc, argv, "a:o:dvn:I:A:D:")) != -1) {
    const char cc = c;

    switch(c) {
    case 'v':
      verbose = true;
      break;

    case 'o':
      output = optarg;
      break;

    case 'a':
      architecture = optarg;
      break;

    case 'd':
      showparse = true;
      yy_flex_debug = 1;
      break;

      /* CPP OPTIONS */
    case 'I':
      /* no spaces in what we hand to cpp: */
      strbuf_append_char(cpp_cmd_args, '-');
      strbuf_append_char(cpp_cmd_args, cc);
      strbuf_append(cpp_cmd_args, app_BuildPath(optarg));
      strbuf_append_char(cpp_cmd_args, ' ');
      break;

    case 'D':
    case 'A':
      /* no spaces in what we hand to cpp: */
      strbuf_append_char(cpp_cmd_args, '-');
      strbuf_append_char(cpp_cmd_args, cc);
      { /* Append optarg, escaping special characters. 
           They will be consumed by popen() below. */
        char * p = optarg;
        while (*p) {
          switch (*p) {
            case '"':
              /* escape with backslash */
              strbuf_append_char(cpp_cmd_args, '\\');
          }
          strbuf_append_char(cpp_cmd_args, *p++);
        }
      }
      strbuf_append_char(cpp_cmd_args, ' ');
      break;
    case 'n':
      if (strcmp(optarg, "ostdinc") == 0) {
	use_std_inc_path = false;
	strbuf_append_char(cpp_cmd_args, '-');
	strbuf_append_char(cpp_cmd_args, cc);
	strbuf_append(cpp_cmd_args, optarg);
	strbuf_append_char(cpp_cmd_args, ' ');
      }
      else if (strcmp(optarg, "ostddef") == 0) {
	use_std_def_list = false;
	strbuf_append_char(cpp_cmd_args, '-');
	strbuf_append_char(cpp_cmd_args, cc);
	strbuf_append(cpp_cmd_args, optarg);
	strbuf_append_char(cpp_cmd_args, ' ');
      }
      else
	opterr++;
      break;
      
    default:
      opterr++;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argc > 1)
    opterr++;

#if 0
  if (argc == 0)
    opterr++;
#endif
  
  if (output == 0)
    opterr++;

  if (!architecture)
    opterr++;

  if (opterr)
    diag_fatal(1, "Usage: mkimage -a architecture -o output [-v] [-d] [-nostdinc] [-Idir]"
		" [-Ddef] [-Aassert] descrip_file\n");

  arch = ExecArch_FromString(architecture);

  image = ei_create();

  if (arch == ExecArch_unknown) {
    diag_fatal(1, "mkimage: unknown architecture \"%s\".\n",
		architecture);
  }

  CurArch = architecture;

  if (use_std_def_list)		/* std defines come first */
    strbuf_append(cpp_cmd, strbuf_asString(stddef));
  
  strbuf_append(cpp_cmd, strbuf_asString(cpp_cmd_args));

  if (use_std_inc_path)		/* std includes come last */
    strbuf_append(cpp_cmd, strbuf_asString(stdinc));

  strbuf_append(cpp_cmd, " -include ");
  strbuf_append(cpp_cmd, 
		app_BuildPath("/eros/host/include/mkimage.preinclude"));
  strbuf_append_char(cpp_cmd, ' ');

  if (argc == 1) {
    current_file = app_BuildPath(argv[0]);
    strbuf_append(cpp_cmd, current_file);
  }
    
#if 0
  fprintf(stderr, "Command to cpp is: %s\n", strbuf_asString(cpp_cmd));
#endif
  
  yyin = popen(strbuf_asString(cpp_cmd), "r");
  
  if (!yyin)
    diag_fatal(1, "Couldn't open description file\n");

  if (verbose)
    app_SetInteractive(true);

  yyparse();

  if (num_errors == 0)
    ei_WriteToFile(image, output);
  
  pclose(yyin);

  if (num_errors != 0u)
    app_SetExitCode(1u);

  ei_destroy(image);
  free(image);

  app_Exit();
  exit(0);
}

int
yywrap()
{
  return 1;
}

void
yyerror(const char * msg)
{
  diag_printf("%s:%d: syntax error\n",
	       current_file, current_line);
  num_errors++;
}

bool
AddRawSegment(ErosImage *image,
	      const char *fileName,
	      KeyBits *segKey)
{
  const char *source = fileName;
  uint32_t nPages;
  unsigned pg;
  uint8_t *buf;  
  struct stat statbuf;

  int sfd = open(source, O_RDONLY);
  if (sfd < 0) {
    diag_printf("Unable to open segment file \"%s\"\n", source);
    return false;
  }

  if (fstat(sfd, &statbuf) < 0) {
    diag_printf("Can't stat segment file \"%s\"\n", source);
    close(sfd);
    return false;
  }

  nPages = statbuf.st_size / EROS_PAGE_SIZE;
  if (statbuf.st_size % EROS_PAGE_SIZE)
    nPages++;

  buf = malloc(nPages * EROS_PAGE_SIZE);
  memset(buf, 0, nPages * EROS_PAGE_SIZE);

  if (read(sfd, buf, statbuf.st_size) != statbuf.st_size) {
    diag_printf("Cannot read segment image \"%s\"\n", source);
    close(sfd);
    return false;
  }

  close(sfd);

  keyBits_InitToVoid(segKey);
  
  for (pg = 0;  pg < nPages; pg++) {
    uint32_t pageAddr = pg * EROS_PAGE_SIZE;
    KeyBits pageKey = ei_AddDataPage(image, &buf[pg * EROS_PAGE_SIZE], false);
    *segKey = ei_AddSubsegToSegment(image, *segKey, pageAddr, pageKey);
  }

  return true;
}

bool
AddZeroSegment(ErosImage *image,
	       KeyBits *segKey,
	       uint32_t nPages)
{
  unsigned pg;

  keyBits_InitToVoid(segKey);
  
  for (pg = 0;  pg < nPages; pg++) {
    uint32_t pageAddr = pg * EROS_PAGE_SIZE;
    KeyBits pageKey = ei_AddZeroDataPage(image, false);
    *segKey = ei_AddSubsegToSegment(image, *segKey, pageAddr, pageKey);
  }

  return true;
}

bool
AddEmptySegment(ErosImage * image,
		KeyBits * segKey,	// key is returned here
		uint32_t nPages)
{
  unsigned pg;
  KeyBits voidKey;

  keyBits_InitToVoid(&voidKey);
  keyBits_InitToVoid(segKey);
  
  for (pg = 0;  pg < nPages; pg++) {
    uint32_t pageAddr = pg * EROS_PAGE_SIZE;
    *segKey = ei_AddSubsegToSegment(image, *segKey, pageAddr, voidKey);
  }

  return true;
}

bool
GetProgramSymbolValue(const char *fileName,
		      const char *symName,
		      uint32_t *value)
{
  ExecImage *ei = xi_create();
  bool ok;

  if ( !xi_SetImage(ei, fileName, 0, 0) ) {
    xi_destroy(ei);
    return false;
  }

  ok = xi_GetSymbolValue(ei, symName, value);

#if 0
  diag_printf("Value of \"%s\" in \"%s\" is 0x%08x\n"
	       "  entry pt is 0x%08x\n", symName,
	       fileName, value,
	       ei->EntryPt());
#endif
  
  xi_destroy(ei);
  return ok;
}

/* This is by far the most complicated segment construction process.
 * The source of complexity is that both ELF and a.out (under suitable
 * conditions) will create one or more pages that are mapped at
 * multiple addresses.  The common one is that the last page of code
 * and the first page of data are usually written in the same physical
 * page and multiply mapped.  In truth, this is a pretty stupid thing
 * to do, since it allows the user to rewrite the last bit of the
 * code segment, but for the sake of compatibility we allow it.
 * 
 * Processing is not as complicated as it looks.
 * 
 * 1. For each unique page in the input file, we add a page to the
 *    ErosImage file.
 * 2. For each MAPPING of a page in the input file, we add a page to
 *    the segment.
 * 
 * The end result is that we make another copy of everything in the
 * ExecImage file, which is arguably unfortunate, but much simpler
 * than the other approaches I've been able to come up with.  Keep in
 * mind that the ExecImage instance is very temporary.
 * 
 * The trick to this is that some pages either will not originate in
 * the binary file or will need to be duplicated 
 */

bool
AddProgramSegment(ErosImage * image,
		  const char * fileName,
                  KeyBits * segKey,
                  uint32_t permMask, uint32_t permValue,
                  bool initOnly)
{
  ExecImage *ei = xi_create();
  KeyBits pageKey;
  PtrMap *map = ptrmap_create();
  unsigned i;
  const ExecRegion * er;
  uint32_t va;    
  uint32_t pageVa;

  keyBits_InitToVoid(segKey);

  if (! xi_SetImage(ei, fileName, permMask, permValue) ) {
    xi_destroy(ei);
    return false;
  }
  
  const uint8_t * const imageBuf = xi_GetImage(ei);

  /* Make two passes over the regions in the executable image.
     The first pass initializes pages with the data from the file.
     The second pass creates zero data pages where there is no
     initialization data from the file. */

  for (i = 0; i < xi_NumRegions(ei); i++) {
    er = xi_GetRegion(ei, i);

#if 0
    char perm[4] = "\0\0\0";
    char *pbuf = perm;
    if (er->perm & ER_R)
      *pbuf++ = 'R';
    if (er->perm & ER_W)
      *pbuf++ = 'W';
    if (er->perm & ER_X)
      *pbuf++ = 'X';

    diag_printf("AddProgramSegment %s "
                  "va=0x%08x   memsz=0x%08x   filesz=0x%08x"
		  "   offset=0x%08x   %s\n",
                fileName,
		er->vaddr, er->memsz, er->filesz, er->offset, perm);
#endif

    bool readOnly = (er->perm & ER_W) == 0;

    uint32_t topfileva = er->vaddr + er->filesz;
      
    for (va = er->vaddr; va < topfileva; va = pageVa + EROS_PAGE_SIZE) {
      pageVa = va & ~EROS_PAGE_MASK;

      uint32_t topva = pageVa + EROS_PAGE_SIZE;
      if (topva > topfileva)
        topva = topfileva;	// take minimum
      unsigned int copySize = topva - va;

      const uint8_t * fileVa = imageBuf + er->offset + (va - er->vaddr);

      /* See if we have already created this page: */
      if (ptrmap_Lookup(map, (char *)0 + pageVa, &pageKey)) {
        /* We have a page key. */
        uint8_t * pageContent = ei_GetPageContentRef(image, &pageKey);

        memcpy(pageContent + va - pageVa, fileVa, copySize);

        /* The page must be writeable if it's writeable by any region: */
        if (! readOnly)
          keyBits_ClearReadOnly(&pageKey);
      }
      else {
        uint8_t pagebuf[EROS_PAGE_SIZE];
        bzero(pagebuf, EROS_PAGE_SIZE);

        memcpy(pagebuf + va - pageVa, fileVa, copySize);

        pageKey = ei_AddDataPage(image, pagebuf, readOnly);
        ptrmap_Add(map, (char *)0 + pageVa, pageKey);
      }

      *segKey = ei_AddSegmentToSegment(image, *segKey, pageVa, pageKey);
    }
  }

  /* Second pass: create zero data pages. */
  if (!initOnly) {
    for (i = 0; i < xi_NumRegions(ei); i++) {
      er = xi_GetRegion(ei, i);

      bool readOnly = (er->perm & ER_W) == 0;

      for (va = er->vaddr + er->filesz;
           va < er->vaddr + er->memsz;
           va = pageVa + EROS_PAGE_SIZE) {
        pageVa = va & ~EROS_PAGE_MASK;
        /* See if we have already created this page: */
        if (ptrmap_Lookup(map, (char *)0 + pageVa, &pageKey)) {
          /* The page has already been added, and any uninitialized
             portion has been zeroed. */
          /* The page must be writeable if it's writeable by any region: */
          if (! readOnly)
            keyBits_ClearReadOnly(&pageKey);
        }
        else {
          /* Add a zero page: */
          pageKey = ei_AddZeroDataPage(image, readOnly);
          ptrmap_Add(map, (char *)0 + pageVa, pageKey);
        }

        *segKey = ei_AddSegmentToSegment(image, *segKey, pageVa, pageKey);
      }
    }
  }

  xi_destroy(ei);
  ptrmap_destroy(map);

  if (keyBits_IsVoidKey(segKey)) {
    diag_error(1, "File \"%s\" has no pages.\n", fileName);
    return false;
  }
  return true;
}

void
ShowImageDirectory(const ErosImage *image)
{
  unsigned i;
  diag_printf("Image directory:\n");

  for (i = 0; i < image->hdr.nDirEnt; i++) {
    EiDirent d = ei_GetDirEntByIndex(image, i);
    diag_printf("  [%2d]", i);
    diag_printf("  %-16s  ", ei_GetString(image, d.name));
    PrintDiskKey(d.key);
    diag_printf("\n");
  }
}

void
ShowImageThreadDirectory(const ErosImage *image)
{
  unsigned i;
  diag_printf("Image threads:\n");

  for (i = 0; i < image->hdr.nStartups; i++) {
    EiDirent d = ei_GetStartupEntByIndex(image, i);
    diag_printf("  [%2d]", i);
    diag_printf("  %-16s  ", ei_GetString(image, d.name));
    PrintDiskKey(d.key);
    diag_printf("\n");
  }
}

#define __EROS_PRIMARY_KEYDEF(name, isValid, bindTo) { #name, isValid },
/* OLD_MISCKEY(name) { #name, 0 }, */

static const struct {
  char *name;
  int isValid;
} KeyNames[KKT_NUM_KEYTYPE] = {
#include <eros/StdKeyType.h>
};

bool
GetMiscKeyType(const char *s, uint32_t *ty)
{
  unsigned i;
  const char *sstr = s;
  
  for (i = 0; i < KKT_NUM_KEYTYPE; i++) {
    if (strcasecmp(sstr, KeyNames[i].name) == 0) {
      if (KeyNames[i].isValid == 0)
	return false;
      if (i < FIRST_MISC_KEYTYPE)
	return false;
      *ty = i;
      return true;
    }
  }

  return false;
}

uint16_t
decimal_value(char c)
{
  /* also suitable for octal, binary */
  return c - '0';
}

uint16_t
hex_value(char c)
{
  if (isdigit(c))
    return c - '0';

  /* else it's a hex alpha */
  return tolower(c) - 'a' + 10;
}

bool
CheckRestriction(uint32_t restriction, KeyBits key)
{
  if ((restriction & RESTRICT_VOID) && !keyBits_IsVoidKey(&key))
    return false;
  
  if ((restriction & RESTRICT_NUMBER) && !keyBits_IsType(&key, KKT_Number))
    return false;
  
  if ((restriction & RESTRICT_SCHED) && !keyBits_IsType(&key, KKT_Sched))
    return false;
  
  if ((restriction & RESTRICT_IOSPACE) && !keyBits_IsType(&key, KKT_DevicePrivs))
    return false;
  
  if ((restriction & RESTRICT_START) && !keyBits_IsType(&key, KKT_Start) &&
      !keyBits_IsVoidKey(&key)) 
    return false;
  
  if ((restriction & RESTRICT_SEGMODE) &&
      ! (keyBits_IsSegModeType(&key) || keyBits_IsType(&key, KKT_Number)))
    return false;
  
  if ((restriction & (RESTRICT_KEYREGS | RESTRICT_GENREGS)) &&
      !keyBits_IsType(&key, KKT_Node))
    return false;

  return true;
}

/* ???APPLIES the qualifications specified by qualifier to the key. */
bool
QualifyKey(uint32_t qualifier, KeyBits key, KeyBits *out)
{
  if (qualifier & ATTRIB_WEAK) {
    switch(keyBits_GetType(&key)) {
    case KKT_Node:
    case KKT_GPT:
      keyBits_SetWeak(&key);
      break;

    case KKT_Number:		/* don't whine about these */
    case KKT_Page:
      break;

    default:
      diag_printf("%s:%d: WEAK qualifier not permitted"
		   " for key type\n",
		   current_file, current_line);
      return false;
    }
  }

  if (qualifier & ATTRIB_RO) {
    switch(keyBits_GetType(&key)) {
    case KKT_Node:
    case KKT_Page:
    case KKT_GPT:
      keyBits_SetReadOnly(&key);
      break;

    case KKT_Number:		/* don't whine about these */
      break;

    default:
      diag_printf("%s:%d: RO qualifier not permitted"
		   " for key type\n",
		   current_file, current_line);
      return false;
    }
  }

  if (qualifier & ATTRIB_NC) {
    switch(keyBits_GetType(&key)) {
    case KKT_Node:
    case KKT_GPT:
      keyBits_SetNoCall(&key);
      break;

    case KKT_Number:		/* don't whine about these */
    case KKT_Page:
      break;
      
    default:
      diag_printf("%s:%d: NC qualifier not permitted"
		   " for key type\n",
		   current_file, current_line);
      return false;
    }
  }

  *out = key;
  return true;
}

KeyBits 
NumberFromString(const char *is)
{
  KeyBits nk;
  uint32_t w0, w1, w2;

  char theText[12];

  keyBits_InitToVoid(&nk);
  bzero(theText, 12);
  strncpy(theText, is, 12);

  w0 = 
    ( ((uint32_t) theText[3]) << 24 |
      ((uint32_t) theText[2]) << 16 |
      ((uint32_t) theText[1]) << 8 |
      ((uint32_t) theText[0]) );
  w1 = 
    ( ((uint32_t) theText[7]) << 24 |
      ((uint32_t) theText[6]) << 16 |
      ((uint32_t) theText[5]) << 8 |
      ((uint32_t) theText[4]) );
  w2 = 
    ( ((uint32_t) theText[11]) << 24 |
      ((uint32_t) theText[10]) << 16 |
      ((uint32_t) theText[9]) << 8 |
      ((uint32_t) theText[8]) );

  init_NumberKey(&nk, w0, w1, w2);
  return nk;
}

bool	// returns false iff error
CheckSubsegOffset(const ErosImage * image, uint64_t offset,
                  KeyBits rootkey, KeyBits subSeg)
{
  uint32_t rootBLSS = ei_GetAnyBlss(image, rootkey);
  uint32_t segBLSS = ei_GetAnyBlss(image, subSeg);
  uint32_t segOffsetBLSS;
  if (offset == 0) {
    segOffsetBLSS = segBLSS;
  } else {
    if (offset & lss_Mask(segBLSS)) {
      diag_printf("%s:%d: Inserted segment cannot be aligned to offset.\n",
                  current_file, current_line);
      return false;
    }
    segOffsetBLSS = lss_BiasedLSS(offset);
  }
  // Now segBLSS <= segOffsetBLSS.
 
  if (segOffsetBLSS == segBLSS && rootBLSS <= segOffsetBLSS) {
    diag_printf("%s:%d: Inserted segment and offset would "
                "replace existing segment.\n",
                current_file, current_line);
    return false;
  }
  return true;
}

