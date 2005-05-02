#ifndef __SPACEBANKKEY_H__
#define __SPACEBANKKEY_H__

/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

#include <idl/eros/Range.h>

#define OC_SpaceBank_AllocNode(count)	       (count)
#define OC_SpaceBank_Alloc1Node		       1
#define OC_SpaceBank_Alloc2Nodes	       2
#define OC_SpaceBank_Alloc3Nodes	       3

#define OC_SpaceBank_ReclaimNode(count)        (3 + (count))
#define OC_SpaceBank_Reclaim1Node              4
#define OC_SpaceBank_Reclaim2Nodes             5
#define OC_SpaceBank_Reclaim3Nodes             6

#define OC_SpaceBank_ReclaimNodesFromNode      7

#define OC_SpaceBank_SeverNode                 8

#define OC_SpaceBank_AllocDataPage(count)     (8 + (count))
#define OC_SpaceBank_Alloc1DataPage           9
#define OC_SpaceBank_Alloc2DataPages          10
#define OC_SpaceBank_Alloc3DataPages          11

#define OC_SpaceBank_ReclaimDataPage(count)   (11 + (count))
#define OC_SpaceBank_Reclaim1DataPage         12
#define OC_SpaceBank_Reclaim2DataPages        13
#define OC_SpaceBank_Reclaim3DataPages        14

#define OC_SpaceBank_ReclaimDataPagesFromNode 15

#define OC_SpaceBank_SeverDataPage            16

/* 16 through 24 were for CapPages.
   Reserve for key tree nodes. */

/* Following are TEMPORARY until I sort out a debugging problem. They
   may be retained if useful. */
#define OC_SpaceBank_IdentifyNode(count)      (24 + (count))
#define OC_SpaceBank_Identify1Node            25
#define OC_SpaceBank_Identify2Nodes           26
#define OC_SpaceBank_Identify3Nodes           27

#define OC_SpaceBank_IdentifyDataPage(count)  (27 + (count))
#define OC_SpaceBank_Identify1DataPage        28
#define OC_SpaceBank_Identify2DataPages       29
#define OC_SpaceBank_Identify3DataPages       30

/* Process stuff */

#define OC_SpaceBank_Preclude             256
 #define SB_PRECLUDE_DESTROY    0x1u
 #define SB_PRECLUDE_LIMITMODS  0x2u

#define OC_SpaceBank_SetLimits            512
#define OC_SpaceBank_GetLimits            513

#define OC_SpaceBank_CreateChild          528
#define OC_SpaceBank_DestroyBankAndSpaces 536

#define OC_SpaceBank_VerifyBank          1024

/* error defines */
#define RC_SB_LimitReached     1	/* spacebank limit reached */

#ifndef __ASSEMBLER__

struct bank_limits {
  uint64_t frameLimit;    /* Maximum number of frames allocatable from
			    this bank (SETTABLE) */
  uint64_t allocCount;    /* Number of frames allocated through this
			    bank and its children */

  uint64_t effFrameLimit; /* Min (the frameLimits of this bank and all
			    of its parents) */

  uint64_t effAllocLimit; /* Min (# of availible frames in this bank and
                            all of its parents) -- The number of frames
                            that can actually be allocated right now */
  uint64_t allocs[eros_Range_otNUM_TYPES];
  uint64_t reclaims[eros_Range_otNUM_TYPES];
};

uint32_t spcbank_buy_nodes(uint32_t krBank, uint32_t count,
			   uint32_t krTo0,  uint32_t krTo1,
			   uint32_t krTo2);
uint32_t spcbank_buy_data_pages(uint32_t krBank, uint32_t count,
				uint32_t krTo0, uint32_t krTo1,
				uint32_t krTo2);

uint32_t spcbank_identify_node(uint32_t krBank, uint32_t krNode);
uint32_t spcbank_identify_data_page(uint32_t krBank, uint32_t krPage);

uint32_t spcbank_return_node(uint32_t krBank, uint32_t krNode);
uint32_t spcbank_return_data_page(uint32_t krBank, uint32_t krPage);

uint32_t spcbank_set_limits(uint32_t krBank,
			    const struct bank_limits *newLimits);
uint32_t spcbank_get_limits(uint32_t krBank,
			    /*OUT*/struct bank_limits *getLimits);

uint32_t spcbank_create_subbank(uint32_t krBank, uint32_t krNewBank);
uint32_t spcbank_create_key(uint32_t krBank, uint32_t precludes,
			    uint32_t krNewKey);

uint32_t spcbank_destroy_bank(uint32_t krBank, uint32_t andSpace);

uint32_t spcbank_verify_bank(uint32_t krVerifyer, uint32_t krBank,
			     uint32_t *isGood);

#endif


#endif /* __SPACEBANKKEY_H__ */

