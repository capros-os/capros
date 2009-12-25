#
# Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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

default: install

CROSS_BUILD=yes
EROS_SRC=../../../..

include $(EROS_SRC)/build/make/makevars.mk

DIRS=lib
DIRS+=storage

TARGETS=$(BUILDDIR)/usbhcd
VPATH=core:host
OBJECTS=
OBJECTS+= $(BUILDDIR)/buffer.o
OBJECTS+= $(BUILDDIR)/config.o
OBJECTS+= $(BUILDDIR)/driver.o
OBJECTS+= $(BUILDDIR)/generic.o
OBJECTS+= $(BUILDDIR)/hcd.o
OBJECTS+= $(BUILDDIR)/hub.o
OBJECTS+= $(BUILDDIR)/message.o
OBJECTS+= $(BUILDDIR)/quirks.o
OBJECTS+=$(BUILDDIR)/usb.o
OBJECTS+= $(BUILDDIR)/urb.o
ifeq "$(EROS_TARGET)" "arm"
OBJECTS+= $(BUILDDIR)/ohci-hcd.o
VPATH+=:arch/arm/mach-ep93xx
OBJECTS+= $(BUILDDIR)/core.o
else
endif
OBJECTS+=$(BUILDDIR)/cap.o

INC=$(DRIVERINC)

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

IDL=$(wildcard *.idl)
interfaces: $(IDL)
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros

$(BUILDDIR)/usbhcd: $(OBJECTS) $(DRIVERDEPS)
	$(DRIVERLINK) $(OBJECTS) $(DRIVERLIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/syms
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 644 usbhcd.map $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers

-include $(BUILDDIR)/.*.m
