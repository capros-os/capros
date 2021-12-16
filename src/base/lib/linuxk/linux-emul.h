#ifndef __LINUXEMUL_H__
#define __LINUXEMUL_H__

/*
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

/* Support for Linux drivers running in CapROS processes. */

#include <domain/Runtime.h>

/* In Linux the following are done in the compilation command: */
#define __KERNEL__	// so we get kernel declarations
#include <linux/autoconf.h>
#define __LINUX_ARM_ARCH__ 4 // FIXME: ARM EP93xx specific!!
#define KBUILD_STR(s) #s
#define KBUILD_BASENAME KBUILD_STR(amba_pl010) // FIXME: ARM EP93xx specific!!
#define KBUILD_MODNAME KBUILD_STR(amba_pl010) // FIXME: ARM EP93xx specific!!

#include <stdint.h>	// need this before linux/kernel.h due to LLONG_MAX

#endif /* __LINUXEMUL_H__ */
