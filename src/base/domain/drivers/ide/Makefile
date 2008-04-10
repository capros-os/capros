#
# Copyright (C) 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, Strawberry Development Group.
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


default: all

CROSS_BUILD=yes

EROS_SRC=../../../..
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include

include $(EROS_SRC)/build/make/makevars.mk
include drivers.mk

GENERATED=$(BUILDDIR)/constituents.h

TARGETS= $(BUILDDIR)/ide_ide

IDE_IDE_OBJECTS=$(BUILDDIR)/ide_ide.o $(BUILDDIR)/ide_drive.o 

OBJECTS=$(IDE_IDE_OBJECTS) 

include $(EROS_SRC)/build/make/makerules.mk

OPTIM=-g

all: $(TARGETS)

ide_ide.c:	     $(BUILDDIR)/constituents.h ide_group.h ide_hwif.h ide_ide.h 
ide_drive.c:         $(BUILDDIR)/constituents.h ide_group.h ide_hwif.h ide_ide.h

$(BUILDDIR)/ide_ide: $(IDE_IDE_OBJECTS) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(IDE_IDE_OBJECTS) $(CROSSLIBS)


install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers/ide
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers/ide
#	$(INSTALL) -m 644 

-include $(BUILDDIR)/.*.m\
