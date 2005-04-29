#
# Copyright (C) 1998, 1999, Jonathan S. Shapiro.
#
# This file is part of the EROS Operating System.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

# Overrides for building under FreeBSD.  We assume that you have installed
# the EROS cross-environment in /usr/local/eros-xenv.  If it is someplace
# else, you'll need to set EROS_XENV as an environment variable.

CROSS_PREFIX=$(EROS_TARGET)-unknown-eros-

EROS_GCC=$(EROS_ROOT)/host/bin/$(CROSS_PREFIX)gcc
EROS_GPLUS=$(EROS_ROOT)/host/bin/$(CROSS_PREFIX)g++
EROS_LD=$(EROS_ROOT)/host/bin/$(CROSS_PREFIX)ld
EROS_AR=$(EROS_ROOT)/host/bin/$(CROSS_PREFIX)ar
EROS_SIZE=$(EROS_ROOT)/host/bin/$(CROSS_PREFIX)size
EROS_OBJDUMP=$(EROS_ROOT)/host/bin/$(CROSS_PREFIX)objdump
EROS_RANLIB=$(EROS_ROOT)/host/bin/$(CROSS_PREFIX)ranlib

EROS_CPP=/lib/cpp -undef -nostdinc -D$(EROS_TARGET)
EROS_GCC_OPTIM=-finline-limit=3000 -fno-strict-aliasing
EROS_GCC_KERNEL_ALIGN=-falign-functions=4

NEED_CROSS_BOOTSTRAP=yes

GAWK=gawk

HOST_FD=/dev/fd0H1440
