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

CROSS_BUILD=yes
EROS_SRC=../../../../../..

include $(EROS_SRC)/build/make/makevars.mk

LWIP_ROOT=../..
include $(LWIP_ROOT)/common.mk

INSTALL_DIR=$(CAPROS_DOMAIN)/drivers/lwip

INC+=-I$(INSTALL_DIR)	# for cap.c to get lwipCap.h

HEADERS=lwipCap.h

TARGETS=$(BUILDDIR)/lwip

OBJECTS=$(COMMON_OBJECTS)
OBJECTS+= $(BUILDDIR)/main.o
OBJECTS+= $(BUILDDIR)/ethInput.o
VPATH+=:$(LWIP_ROOT)/arch/$(LINUX2624_TARGET)/$(CAPROS_MACH)
# from arch/$(LINUX2624_TARGET)/$(CAPROS_MACH):
OBJECTS+= $(BUILDDIR)/dev.o

include $(EROS_SRC)/build/make/makerules.mk

$(BUILDDIR)/lwip: $(OBJECTS) $(DRIVERDEPS)
	$(DRIVERLINK) $(OBJECTS) $(DRIVERLIBS) -o $@
# and for debugging:
	$(TARGET_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/syms
	$(TARGET_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

interfaces:
	$(INSTALL) -d $(INSTALL_DIR)
	$(INSTALL) -m 644 $(HEADERS) $(INSTALL_DIR)

install: $(TARGETS)
	$(INSTALL) -d $(INSTALL_DIR)
	$(INSTALL) -m 644 $(CAPROS_MACH)/lwip.map ep93xx.map $(INSTALL_DIR)
	$(INSTALL) -m 755 $(TARGETS) $(INSTALL_DIR)

-include $(BUILDDIR)/.*.m

