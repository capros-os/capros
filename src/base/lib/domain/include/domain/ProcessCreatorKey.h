#ifndef __PROCESSCREATORKEY_H__
#define __PROCESSCREATORKEY_H__

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

#define OC_ProcCre_CreateProcess		 1
#define OC_ProcCre_DestroyProcess		 2
#define OC_ProcCre_DestroyCallerAndReturn 3

#define OC_ProcCre_RemoveDestroyRights    8

#define OC_ProcCre_AmplifyGateKey         16
#define OC_ProcCre_AmplifySegmentKey      17

#define RC_ProcCre_BadBank                1
#define RC_ProcCre_Paternity              2
#define RC_ProcCre_WrongBank              3

#ifndef __ASSEMBLER__
uint32_t proccre_destroy_process(uint32_t krProcCre, uint32_t krBank, uint32_t krDom);
uint32_t proccre_create_process(uint32_t krProcCre, uint32_t krBank, uint32_t krYield);
uint32_t proccre_amplify_gate(uint32_t krProcCre, uint32_t krGate,
			      uint32_t krProc, uint32_t *capType,
			      uint32_t *capInfo);
#endif

#endif /* __PROCESSCREATORKEY_H__ */
