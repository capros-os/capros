#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

default: package
EROS_SRC=../../..
include $(EROS_SRC)/build/make/makevars.mk

HTML_DOCS= \
	00DesignNotes.html \
	AutoConf.html \
	DiskFormatting.html \
	DomainLayout.html \
	KernelMemory.html \
	ContextLoad.html \
	IPCv2.html \
	IPCv3.html \
	LineDiscDesign.html \
	SmallSpaces386.html \
	POSIX.html \
	Checkpoint.html \
	Keeper.html \
	SpaceBank.html \
	BootSector386.html \
	CapabilityPages.html \
	CapInvoke.html \
	UserDrivers.html \
	KernelStacks.html \
	EROSv2.html \
	KernMalloc.html \
	KernOutput.html \
	IA32-Emulation.html \
	ReviewingWhereWeStand.html \

FIGS=$(notdir $(wildcard ./*.fig))
GENERATED=$(FIGS:.fig=.gif)

OTHER_DOCS= \
	ConsistencyMgmt.ps \
	$(GENERATED)

DIRS=DependencyMgmt EtherLink
CLEANLIST=$(GENERATED)

include $(EROS_SRC)/build/make/makerules.mk

install: local-install

local-install: $(GENERATED)
	$(INSTALL) -d $(EROS_ROOT)/doc
	$(INSTALL) -d $(EROS_ROOT)/doc/www
	$(INSTALL) -d $(EROS_ROOT)/doc/www/design-notes
	$(INSTALL) -h -m 0644 $(HTML_DOCS) $(EROS_ROOT)/doc/www/design-notes
	$(INSTALL) -m 0644 $(OTHER_DOCS) $(EROS_ROOT)/doc/www/design-notes

-include DEPEND
