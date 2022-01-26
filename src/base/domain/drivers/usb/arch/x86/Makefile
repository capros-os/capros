#
# Copyright (C) 2010, Strawberry Development Group.
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

include ../../common.mk
include $(EROS_SRC)/build/make/makevars.mk

OBJECTS=$(COMMON_OBJECTS)

OBJECTS+= $(BUILDDIR)/hcd-pci.o
TARGETS=$(BUILDDIR)/usbhcdehci ### ////$(BUILDDIR)/usbhcduhci
TARGETS+=$(BUILDDIR)/usb_pci_ids.o # for pci registry
MAPS=../../host/pciehci.map ### //// ../../host/pciuhci.map
MAPS+=usbhcd.map

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/usbhcduhci: $(OBJECTS) $(UHCI_OBJECTS) $(DRIVERDEPS)
	$(DRIVERLINK) $(OBJECTS) $(UHCI_OBJECTS) $(DRIVERLIBS) -o $@
# and for debugging:
	$(TARGET_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/symsuhci
	$(TARGET_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembleduhci

$(BUILDDIR)/usbhcdehci: $(OBJECTS) $(EHCI_OBJECTS) $(DYNDRIVERDEPS)
	$(DYNDRIVERLINK) $(OBJECTS) $(EHCI_OBJECTS) $(DRIVERLIBS) -o $@
# and for debugging:
	$(TARGET_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/symsehci
	$(TARGET_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembledehci

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 644 $(MAPS) $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers

-include $(BUILDDIR)/.*.m
