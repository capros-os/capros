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

CROSS_PREFIX=$(EROS_TARGET)-unknown-linux-
XENV_INCLUDE=-I$(EROS_XENV)/include
XENV_LIBDIR=-L$(EROS_XENV)/lib

TARGET_GCC=$(EROS_XENV)/bin/$(CROSS_PREFIX)gcc
TARGET_GPLUS=$(EROS_XENV)/bin/$(CROSS_PREFIX)g++
TARGET_LD=$(EROS_XENV)/bin/$(CROSS_PREFIX)ld
TARGET_AR=$(EROS_XENV)/bin/$(CROSS_PREFIX)ar
TARGET_SIZE=$(EROS_XENV)/bin/$(CROSS_PREFIX)size
TARGET_OBJDUMP=$(EROS_XENV)/bin/$(CROSS_PREFIX)objdump
TARGET_RANLIB=$(EROS_XENV)/bin/$(CROSS_PREFIX)ranlib

# GCC 2.8+ and EGCE require these to successfully compile the kernel:
TARGET_GPLUS_OPTIM=-fno-rtti -fno-exceptions
NATIVE_GPLUS_OPTIM=

GAWK=awk
ifndef NETPBMDIR
NETPBMDIR=/usr/X11R6/bin
endif

HOST_FD=/dev/rfd0
