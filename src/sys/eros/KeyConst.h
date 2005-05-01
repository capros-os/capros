#ifndef __EROS_KEY_H__
#define __EROS_KEY_H__
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
#define SEGMODE_RO		_ASM_U(0x8000)
#define SEGMODE_NC		_ASM_U(0x4000)
#define SEGMODE_WEAK		_ASM_U(0x2000)
#define SEGMODE_BLSS_MASK	_ASM_U(0x1fff)
#define SEGMODE_ATTRIBUTE_MASK  _ASM_U(0xe000)

#define SEGPRM_RO	   _ASM_U(0x4)
#define SEGPRM_NC	   _ASM_U(0x2)
#define SEGPRM_WEAK	   _ASM_U(0x1)

/* Slots of a Red Segment:

   12: By CONVENTION where the red seg keeper puts its space bank
   13: Keeper Key (variable - determined by format key)
   14: Background Key (variable - determined by format key)
   15: Format Key
   */

/* Slots of a process root. Changes here should be matched in the
 * architecture-dependent layout files and also in the mkimage grammar
 * restriction checking logic. */
#define ProcSched             0
#define ProcKeeper            1
#define ProcAddrSpace         2
#define ProcCapSpace          3	/* unimplemented */
#define ProcGenKeys           3 /* for now */
#define ProcIoSpace           4	/* unimplemented */
#define ProcSymSpace          5
#define ProcBrand             6
#define ProcLastInvokedKey    7
#define ProcTrapCode          8
#define ProcPCandSP           9
#define ProcFirstRootRegSlot  8
#define ProcLastRootRegSlot   31


/* #define ProcAltMsgBuf         6 */

#define WrapperSpace          0
#define WrapperFilter         28
#define WrapperBackground     29
#define WrapperKeeper         30
#define WrapperFormat         31

#define WRAPPER_SEND_NODE     _ASM_U(0x00010000)
#define WRAPPER_SEND_WORD     _ASM_U(0x00020000)
#define WRAPPER_KEEPER        _ASM_U(0x00040000)
#define WRAPPER_BACKGROUND    _ASM_U(0x00080000)
#define WRAPPER_BLOCKED       _ASM_U(0x00100000)

#define WRAPPER_SET_BLSS(nkv, blss) \
  do { \
    (nkv).value[0] &= ~SEGMODE_BLSS_MASK; \
    (nkv).value[0] |= (uint32_t)(blss); \
  } while (0) 
#define WRAPPER_GET_BLSS(nkv) \
      ( (nkv).value[0] & SEGMODE_BLSS_MASK )

#endif /* __EROS_KEY_H__ */
