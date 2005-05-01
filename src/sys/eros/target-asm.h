#ifndef __TARGET_ASM_H__
#define __TARGET_ASM_H__

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

/*
 * Basic type declarations for the target platform, used pervasively
 * within the kernel code and the volume construction utilities.
 *
 * This file is included from both C and C++ code, so it needs
 * to be handled carefully.
 */

#define BYTE_MAX        255u

/* In one sense the following declaration is silly.  We use it because
   it is much easier to find 32-bit assumptions that way. */
#define UINT32_BITS     32
#define UINT64_BITS     64

#ifndef UINT32_MAX

#define UINT32_MAX      4294967295u
#define UINT64_MAX      18446744073709551615ull

#endif

#ifndef INT32_MAX

#define INT32_MAX       2147483647
#define INT32_MIN       (- INT32_MAX - 1)

#endif

#define EROS_KEYDATA_MAX 65535u

#include <eros/machine/target-asm.h>

#endif /* __TARGET_ASM_H__ */
