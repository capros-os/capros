#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, Strawberry Development Group
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
default: install

EROS_SRC=..
include $(EROS_SRC)/build/make/makevars.mk

# Note: machine-independent dirs should be built before architecture
# specific directories. Therefore, arch/$(EROS_TARGET) needs to be last!

ETAGDIRS=IO arch ddb disk eros kernel kerninc key libkey pci ckpt
ETAGDIRS+= arch/$(EROS_TARGET)/ddb
ETAGDIRS+= arch/$(EROS_TARGET)/device
ETAGDIRS+= arch/$(EROS_TARGET)/eros
ETAGDIRS+= arch/$(EROS_TARGET)/include
ETAGDIRS+= arch/$(EROS_TARGET)/kernel
ETAGDIRS+= arch/$(EROS_TARGET)/machine

DIRS=eros arch/$(EROS_TARGET)
# Directory libkey is located here, but is actually built in base/lib/domain.
#CLEANDIRS=eros

include $(EROS_SRC)/build/make/pkgrules.mk

#
# DEPEND doesn't really require symlinks, but all of the subordinate
# ones do.  Create the symlink here so we can do recursive depends.
#

nonrecursiveClean:
	find . -name '*~' -exec rm {} \;
