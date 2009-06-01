#ifndef __EROS_KEY_H__
#define __EROS_KEY_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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
  
     o In schedule keys, it holds the priority.
  
*/

#define SEGPRM_RO	   0x4
#define SEGPRM_NC	   0x2
#define SEGPRM_WEAK	   0x1

#endif /* __EROS_KEY_H__ */
