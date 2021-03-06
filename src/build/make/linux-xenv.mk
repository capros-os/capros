#
# Copyright (C) 1998, 1999, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

# Cross tools use i386 not i486.
ifeq "$(EROS_TARGET)" "i486"
EROS_CROSS_TARGET=i386
else
EROS_CROSS_TARGET=$(EROS_TARGET)
endif
CROSS_PREFIX=$(EROS_CROSS_TARGET)-unknown-capros-

TARGET_GCC=$(EROS_XENV)/bin/$(CROSS_PREFIX)gcc
TARGET_GPLUS=$(EROS_XENV)/bin/$(CROSS_PREFIX)g++
TARGET_LD=$(EROS_XENV)/bin/$(CROSS_PREFIX)ld
TARGET_AR=$(EROS_XENV)/bin/$(CROSS_PREFIX)ar
TARGET_SIZE=$(EROS_XENV)/bin/$(CROSS_PREFIX)size
TARGET_OBJCOPY=$(EROS_XENV)/bin/$(CROSS_PREFIX)objcopy
TARGET_OBJDUMP=$(EROS_XENV)/bin/$(CROSS_PREFIX)objdump
TARGET_RANLIB=$(EROS_XENV)/bin/$(CROSS_PREFIX)ranlib
TARGET_STRIP=$(EROS_XENV)/bin/$(CROSS_PREFIX)strip

ifeq "$(EROS_TARGET)" "i486"
TARGET_GCC_KERNEL_ALIGN=-falign-functions=4
endif

GAWK=gawk

HOST_FD=/dev/fd0H1440
