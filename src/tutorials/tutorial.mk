#
# Copyright (C) 2001, The EROS Group, LLC.
# Copyright (C) 2005, 2007, 2008, Strawberry Development Group
#
# This file is part of the CapROS Operating System,
# and is derived from the EROS Operating System.
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

# This material is based upon work supported by the US Defense Advanced
# Research Projects Agency under Contract No. W31P4Q-07-C-0070.
# Approved for public release, distribution unlimited.

# mk -- should be included right after makerules.mk

#CLEANLIST+= sysimg sysvol zsysvol .sysimg.m.* tstflop ztstflop vmfloppy
#CLEANLIST+= mkimage.out

ifndef VOLMAP
VOLMAP=../volmap
endif

ifndef VOLMAPFD  # volmap for floppy disk
VOLMAPFD=../volmapfd
endif

OPTIM=-O
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include

# Following is picked up from environment variable if present.
KERNDIR=$(CAPROS_DOMAIN)/image
ifeq "" "$(findstring $(EROS_CONFIG).eros.debug,$(wildcard $(KERNDIR)/*))"
KERNEL=$(EROS_CONFIG).eros
else
KERNEL=$(EROS_CONFIG).eros.debug
endif
KERNPATH=$(KERNDIR)/$(KERNEL)

include $(EROS_SRC)/build/make/sys.$(EROS_TARGET).mk
