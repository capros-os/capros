/*
 * Copyright (C) 2008, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <linux/errno.h>

#include <domain/assert.h>
#include <eros/Invoke.h>
#include <idl/capros/Errno.h>
unsigned long capros_Errno_ExceptionToErrno(unsigned long excep);
unsigned long capros_Errno_ErrnoToException(unsigned long errno);

#define ErrnosToTranslate \
  ErrnoX(Perm, PERM) \
  ErrnoX(NoEnt, NOENT) \
  ErrnoX(Again, AGAIN) \
  ErrnoX(NoMem, NOMEM) \
  ErrnoX(NoDev, NODEV) \
  ErrnoX(Inval, INVAL) \
  ErrnoX(NoSpc, NOSPC) \
  ErrnoX(MsgSize, MSGSIZE) \
  ErrnoX(ConnReset, CONNRESET) \
  ErrnoX(Shutdown, SHUTDOWN) \
  ErrnoX(HostUnreach, HOSTUNREACH) \

unsigned long
capros_Errno_ExceptionToErrno(unsigned long excep)
{
#define ErrnoX(cap, lin) case RC_capros_Errno_##cap: return E##lin;
  switch (excep) {
    ErrnosToTranslate
  default: return 0;
  };
#undef ErrnoX
}

unsigned long
capros_Errno_ErrnoToException(unsigned long errno)
{
#define ErrnoX(cap, lin) case E##lin: return RC_capros_Errno_##cap;
  switch (errno) {
    ErrnosToTranslate
  default: return 0;
  };
#undef ErrnoX
}
