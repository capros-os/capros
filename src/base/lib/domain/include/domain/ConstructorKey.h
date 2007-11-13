#ifndef __CONSTRUCTOR_H__
#define __CONSTRUCTOR_H__

/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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

/* This file is retained until all the users of constructor_request
   are changed to use the IDL declarations. */

#include <idl/capros/Constructor.h>

#ifndef __ASSEMBLER__
static inline uint32_t
constructor_request(uint32_t krConstructor, uint32_t krBank,
		     uint32_t krSched, uint32_t krArg0,
		     uint32_t krProduct /* OUT */)
{
  return capros_Constructor_request(krConstructor,
           krBank, krSched, krArg0, krProduct);
}
#endif

#endif /* __CONSTRUCTOR_H__ */

