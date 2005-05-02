#ifndef __LINEDISCKEY_H__
#define __LINEDISCKEY_H__

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

#include <eros/CharSrcProto.h>

/* Local Variables: */
/* comment-column:34 */

/* ORDER code values: */
   /* see CharSrcProto.h */

/* RESULT code values */
   /* see CharSrcProto.h */

/* CONTROL code values */
   /* see also CharSrcProto.h */
#define LD_Control_SetInpProcessing  0xCAFE0001
#define LD_Control_SetOutpProcessing 0xCAFE0002
#define LD_Control_SetControlChars   0xCAFE0004

#define LD_Control_GetInpProcessing  0xCAFE0011
#define LD_Control_GetOutpProcessing 0xCAFE0012
#define LD_Control_GetControlChars   0xCAFE0014

/* Input processing flags */
#define LD_In_DoInpProc 0x00000001 /* enable input processing      */

#define LD_In_LineMode  0x00000002 /* enables line buffering       */

#define LD_In_Echo      0x00000010 /* echo incoming characters     */
#define LD_In_EchoCtrl  0x00000020 /* echo control chars as: ^C    */
#define LD_In_AlwEchoNL 0x00000040 /* always echo newlines         */

#define LD_In_VisErase  0x00000100 /* Erase1/2 visually erases when echoed */
#define LD_In_VisKill   0x00000200 /* KillLine/Word visually kill letters  */

#define LD_In_SoftFlow  0x00001000 /* enable soft flow control on input */

#define LD_In_SwapCRNL  0x00010000 /* enable CR->NL, NL->CR swap   */

#define LD_In_DefaultCookedFlags (LD_In_DoInpProc|LD_In_LineMode|LD_In_Echo \
                                  |LD_In_EchoCtrl|LD_In_VisErase            \
                                  |LD_In_VisKill|LD_In_SwapCRNL)

/* Output processing flags */
#define LD_Out_DoOutpProc 0x00000001 /* enable output processing     */

#define LD_Out_NLtoCRNL   0x00000002 /* NL -> CRNL                   */
#define LD_Out_CRtoNL     0x00000004 /* CR -> NL (w/ above, -> CRNL) */
#define LD_Out_NLtoCR     0x00000008 /* NL -> CR  (never elsewise)   */

#define LD_Out_DefaultCookedFlags (LD_Out_DoOutpProc|LD_Out_NLtoCRNL)

/* Control character positions */
#define LD_CC_Erase            0  /* First erase character  */
#define LD_CC_EraseDef     '\x08' /* Default ^H             */

#define LD_CC_AltErase         1  /* Second erase character */
#define LD_CC_AltEraseDef  '\x7F' /* Default ^?             */

#define LD_CC_EraseWord        2  /* Erase word char        */
#define LD_CC_EraseWordDef '\x17' /* Default ^W             */

#define LD_CC_KillLine         3  /* Kill input line char   */
#define LD_CC_KillLineDef  '\x15' /* Default ^U             */

#define LD_CC_Reprint          4  /* Reprint line char      */
#define LD_CC_ReprintDef   '\x12' /* Default ^R             */

#define LD_CC_Quote            5  /* Take next char verbatim*/
#define LD_CC_QuoteDef     '\x16' /* Default ^V             */

#define LD_CC_SFStop           6  /* Soft flow stop char    */
#define LD_CC_SFStopDef    '\x13' /* Default ^S             */

#define LD_CC_SFStart          7  /* Soft flow start char   */
#define LD_CC_SFStartDef   '\x11' /* Default ^Q             */
          /* if LD_CC_SFStart == 0, any character restarts */

#define LD_Num_CC             16  /* 8->15 reserved         */

#ifndef __ASSEMBLER__

uint32_t
linedisc_reset(uint32_t krLineDisc);
/* Resets krLineDisc to initial state */

uint32_t
linedisc_make_raw(uint32_t krLineDisc);

uint32_t
linedisc_make_cooked(uint32_t krLineDisc,
                     uint16_t *specChars,
                     uint32_t numChars);

uint32_t
linedisc_get_inp_proc(uint32_t krLineDisc,
		      uint32_t *inpFlags);
uint32_t
linedisc_get_outp_proc(uint32_t krLineDisc,
		       uint32_t *outpFlags);
uint32_t
linedisc_get_control_chars(uint32_t krLineDisc,
			   uint16_t *chars, uint32_t max,
			   uint32_t *actual);

uint32_t
linedisc_set_inp_proc(uint32_t krLineDisc,
		      uint32_t inpFlags);
uint32_t
linedisc_set_outp_proc(uint32_t krLineDisc,
		       uint32_t outpFlags);
uint32_t
linedisc_set_control_chars(uint32_t krLineDisc,
			   const uint16_t *chars, uint32_t num);

#endif /* __ASSEMBLER__ */
		      
#endif /* __LINEDISCKEY_H__ */


