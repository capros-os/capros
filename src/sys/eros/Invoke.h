#ifndef __EROS_INVOKE_H__
#define __EROS_INVOKE_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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
extern fixreg_t SEND(Message*);
extern fixreg_t RETURN(Message*);

  /* Generic form: */
extern fixreg_t INVOKECAP(Message*);

#ifdef __cplusplus
}
#endif

#endif /* !ASSEMBLER */

/* INVOCATION TYPES */

#define IT_NPReturn 0
#define IT_PReturn  1
#define IT_Call     2
#define IT_Send     3

#define IT_NUM_INVTYPES 4

/* Predefinition of KR_VOID is a kernel matter */
#define KR_VOID  0

/* see ObRef introduction */ 
#define RC_OK   	    0u

#endif /* __EROS_INVOKE_H__ */
