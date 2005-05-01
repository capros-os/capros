#ifndef __CHRSRCPROTO_H__
#define __CHRSRCPROTO_H__

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

/* Local Variables: */
/* comment-column:34 */
/* End: */

/* ORDER code values: */

#define OC_CharSrc_Write               1
#define OC_CharSrc_WaitForEvent        2
#define OC_CharSrc_PostEvent           3
#define OC_CharSrc_Control             4

/* RESULT code values */
#define RC_CharSrc_OneWaiterOnly       1

/* CONTROL code values */
#define CharSrc_Control_GetTimeout       1
#define CharSrc_Control_SetTimeout       2

#define CharSrc_Control_GetSpecialChars  4
#define CharSrc_Control_SetSpecialChars  5

#define CharSrc_Control_FlushInput       8
#define CharSrc_Control_FlushOutput      9
#define CharSrc_Control_FlushInOut      10

/* Event bits -- max 32 possible events */
#define CharSrc_FilledBufferEvent     0x00000001
#define CharSrc_TimeOutEvent          0x00000002
#define CharSrc_SpecialCharEvent      0x00000004
#define CharSrc_WriteSpaceAvailEvent  0x00000008

/* user events */
#define CharSrc_NumUserEvents         16
#define CharSrc_UserEventBase         0x00010000 /* space for 16 events */
#define CharSrc_UserEvent(x)          ((x)*CharSrc_UserEventBase)
#define CharSrc_UserEventMask         0xFFFF0000

/* should == bitwise OR of all events */
#define CharSrc_ALL_Events            0xFFFF000F

#ifndef __ASSEMBLER__

uint32_t charsrc_write(uint32_t krChrsrc,
		   uint32_t len, const char *buff,
		   uint32_t *charsWritten);

uint32_t charsrc_wait_for_event(uint32_t krChrsrc,
			    uint32_t maxLen, char *buff,
			    uint32_t *charsRead,
			    uint32_t eventMask, uint32_t *eventsOccured);

uint32_t charsrc_post_event(uint32_t krChrsrc, uint32_t eventMask);

uint32_t charsrc_control(uint32_t krChrsrc, uint32_t controlCode,
		     uint32_t regArg1, uint32_t regArg2,
		     const void *sndData, uint32_t sndLen,
		     uint32_t *rcvReg1, uint32_t *rcvReg2,
		     void *rcvData, uint32_t maxLen,
		     uint32_t *rcvLen);

/* wrapper around charsrc_control */
uint32_t charsrc_set_timeout(uint32_t krChrsrc,
		         uint32_t initChars, uint32_t timeoutMs);

uint32_t charsrc_get_timeout(uint32_t krChrSrc,
			 uint32_t *curInitChars, uint32_t *curTimeoutMsec);

/* Sleazy: this works for UNICODE too... (wrappers around charsrc_control) */
uint32_t charsrc_get_special_chars(uint32_t krChrsrc,
			       uint16_t *chars, uint32_t limit, uint32_t *actual);

uint32_t charsrc_set_special_chars(uint32_t krChrsrc,
			       const uint16_t *chars, uint32_t num);

#endif /* __ASSEMBLER__ */
		      
#endif /* __CHRSRCPROTO_H__ */
