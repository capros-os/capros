#ifndef __EROS_INVOKE_H__
#define __EROS_INVOKE_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

/*
 * This file resides in eros/ because the kernel and the invocation
 * library must agree on the values.
 */

#if !defined(__ASSEMBLER__)

#include "machine/Invoke.h"

#ifdef __cplusplus
extern "C" {
#endif

extern fixreg_t CALL(Message*);
extern void SEND(Message*);
extern void PSEND(Message*);
extern fixreg_t RETURN(Message*);
extern fixreg_t NPRETURN(Message*);

  /* Generic form: */
extern fixreg_t INVOKECAP(Message*);

#ifdef __cplusplus
}
#endif

#endif /* !ASSEMBLER */

/* INVOCATION TYPES */
/* N.B.: invType_IsCall relies on this encoding. */

#define IT_Return 0
#define IT_PReturn  1
#define IT_Call     2
#define IT_PCall    3
#define IT_Send     4
#define IT_PSend    5

#define IT_NUM_INVTYPES 6

/* Predefinition of KR_VOID is a kernel matter */
#define KR_VOID  0

/* see ObRef introduction */ 
#define RC_OK 0

#endif /* __EROS_INVOKE_H__ */
