#ifndef __REGISTERS_ARM_H__
#define __REGISTERS_ARM_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

/* Architecture-specific register set structure */

struct Registers {
  uint32_t arch;			/* architecture */
  uint32_t len;			/* length of returned structure */
  uint32_t pc;
  uint32_t sp;
  uint32_t faultCode;
  uint32_t faultInfo;
  uint32_t domFlags;
  
  /* architecture-specific registers start here: */
  /* sp and pc are repeated here for convenience when reading.
     When writing, the values above will take effect. */
  uint32_t registers[16];
  uint32_t CPSR;
};

#endif /* __REGISTERS_ARM_H__ */
