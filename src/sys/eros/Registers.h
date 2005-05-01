#ifndef __REGISTERS_H__
#define __REGISTERS_H__

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

/* Architecture neutral overlay for 32 bit architecture registers */

#define ARCH_I386   0xb00000b0

struct CommonRegisters32 {
  uint32_t arch;			/* architecture */
  uint32_t len;			/* length of returned structure */
  uint32_t pc;
  uint32_t sp;
  uint32_t faultCode;
  uint32_t faultInfo;
  uint32_t procState;
  uint32_t procFlags;
};

#endif /*  __REGISTERS_H__ */
