/*
 * Copyright (C) 2006, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */
	
#define SWI_Invoke	0
#define SWI_CSwap32	1
#define SWI_CopyKeyReg	2
#define SWI_XchgKeyReg	3
#define SWI_Bpt		4
#define SWI_PutIRQ      5 // requires I/O privileges
#define SWI_DisableIRQ  6 // requires I/O privileges
#define SWI_EnableIRQ   7 // requires I/O privileges, not enforced
#define SWI_MaxSWI	7
#define SWI_halt	255
