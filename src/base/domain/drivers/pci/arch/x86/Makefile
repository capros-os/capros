#
# Copyright (C) 2009, Strawberry Development Group.
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

default: install

CROSS_BUILD=yes
EROS_SRC=../../../../../..

include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/pci
OBJECTS=
OBJECTS+=$(BUILDDIR)/caprosInit.o
OBJECTS+=$(BUILDDIR)/i386.o
OBJECTS+=$(BUILDDIR)/init.o
OBJECTS+=$(BUILDDIR)/common.o
OBJECTS+= $(BUILDDIR)/direct.o
OBJECTS+= $(BUILDDIR)/irq.o
OBJECTS+= $(BUILDDIR)/legacy.o
//OBJECTS+= $(BUILDDIR)/mmconfig-shared.o

VPATH=../..
OBJECTS+= $(BUILDDIR)/access.o
OBJECTS+= $(BUILDDIR)/bus.o
OBJECTS+= $(BUILDDIR)/cap.o
OBJECTS+= $(BUILDDIR)/pci.o
OBJECTS+= $(BUILDDIR)/pci-driver.o
OBJECTS+= $(BUILDDIR)/probe.o
OBJECTS+= $(BUILDDIR)/search.o
OBJECTS+= $(BUILDDIR)/setup-res.o
OBJECTS+= $(BUILDDIR)/stubs.o ##

INC=$(DRIVERINC)

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/pci: $(OBJECTS) $(DRIVERDEPS)
	$(DRIVERLINK) $(OBJECTS) $(DRIVERLIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers

-include $(BUILDDIR)/.*.m
