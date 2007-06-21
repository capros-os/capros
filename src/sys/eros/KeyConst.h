#ifndef __EROS_KEY_H__
#define __EROS_KEY_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#ifndef __STDKEYTYPE_H__
#include <eros/StdKeyType.h>
#endif

/*
   The 'keyData' field of the key is used for a number of purposes:
  
     o In start keys, it provides the process with an indicator of
       which key was invoked.
  
     o In segmode keys, it contains the biased log segment size and
       the permissions, organized as follows:
  
          bit position: 15  14  13 12:5 4:0
                        RO  NC  WK   0   blss
  
       bits 12:5 are reserved.  RO and NC stand for "read only" and
       "don't call" respectively.  TY indicates whether a segment key
       is a red or black segment key. 
  
     o In schedule keys, it holds the priority.
  
     o In the old key format, it was used to hold the key subtype in
       device and miscellaneous keys.  In the new format, this
       information is now stored in the 'subType' field.
*/
/* User-visible attributes of a key: */
#define SEGMODE_RO		0x8000
#define SEGMODE_NC		0x4000
#define SEGMODE_WEAK		0x2000
#define SEGMODE_BLSS_MASK	0x1fff
#define SEGMODE_ATTRIBUTE_MASK  0xe000

#define SEGPRM_RO	   0x4
#define SEGPRM_NC	   0x2
#define SEGPRM_WEAK	   0x1

/* Slots of a Red Segment:

   13: Keeper Key (variable - determined by format key)
   14: Background Key (variable - determined by format key)
   15: Format Key
   */

#define WrapperBackground     29
#define WrapperKeeper         30
#define WrapperFormat         31

#define WRAPPER_SEND_NODE     0x00010000
#define WRAPPER_SEND_WORD     0x00020000
#define WRAPPER_KEEPER        0x00040000
#define WRAPPER_BACKGROUND    0x00080000
#define WRAPPER_BLOCKED       0x00100000

#define WRAPPER_SET_BLSS(nkv, blss) \
  do { \
    (nkv).value[0] &= ~SEGMODE_BLSS_MASK; \
    (nkv).value[0] |= (uint32_t)(blss); \
  } while (0) 
#define WRAPPER_GET_BLSS(nkv) \
      ( (nkv).value[0] & SEGMODE_BLSS_MASK )

#endif /* __EROS_KEY_H__ */
