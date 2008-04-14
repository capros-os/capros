#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2007, 2008, Strawberry Development Group.
#
# This file is part of the CapROS Operating System.
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

default: package
EROS_SRC=../../..
include $(EROS_SRC)/build/make/makevars.mk

HTML_DOCS= \
	00DesignNotes.html \
	AutoConf.html \
	Checkpoint.html \
	CheckpointLog.svg \
	DiskFormatting.html \
	KernelMemory.html \
	ContextLoad.html \
	IndirectData.html \
	IPCv2.html \
	IPCv3.html \
	LineDiscDesign.html \
	LinuxDrivers.html \
	Multiboot.html \
	SmallSpaces386.html \
	POSIX.html \
	Keeper.html \
	SpaceBank.html \
	BootSector386.html \
	CapabilityPages.html \
	CapInvoke.html \
	UserDrivers.html \
	EROSv2.html \
	KernMalloc.html \
	KernOutput.html \
	IA32-Emulation.html \

FIGS=$(notdir $(wildcard ./*.fig))
GENERATED=$(FIGS:.fig=.gif)

OTHER_DOCS= \
	$(GENERATED)

DIRS=DependencyMgmt EtherLink
CLEANLIST=$(GENERATED)

include $(EROS_SRC)/build/make/makerules.mk

install: local-install

local-install: $(GENERATED)
	$(INSTALL) -d $(EROS_ROOT)/doc/www/design-notes
	$(INSTALL) -m 0644 $(HTML_DOCS) $(EROS_ROOT)/doc/www/design-notes
#	$(INSTALL) -m 0644 $(OTHER_DOCS) $(EROS_ROOT)/doc/www/design-notes

-include DEPEND
