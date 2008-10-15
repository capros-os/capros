#
# Copyright (C) 2008, Strawberry Development Group.
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

TARGETS=$(BUILDDIR)/scsi
OBJECTS=$(BUILDDIR)/cap.o
OBJECTS+=$(BUILDDIR)/disk.o
OBJECTS+=$(BUILDDIR)/scsi.o
OBJECTS+=$(BUILDDIR)/hosts.o
OBJECTS+=$(BUILDDIR)/scsi_devinfo.o
OBJECTS+=$(BUILDDIR)/scsi_error.o
OBJECTS+=$(BUILDDIR)/scsi_scan.o
OBJECTS+=$(BUILDDIR)/scsi_sysfs.o
OBJECTS+=$(BUILDDIR)/scsi_lib.o
OBJECTS+=$(BUILDDIR)/sd.o
OBJECTS+=$(BUILDDIR)/stubs.o

INC=-I$(EROS_ROOT)/include/linux-headers -I$(EROS_ROOT)/include
INC+= -include $(EROS_ROOT)/include/linuxk/linux-emul.h
INC+=-I$(EROS_ROOT)/host/include # for disk/*

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

IDL=$(wildcard *.idl)
interfaces: $(IDL)
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros

$(BUILDDIR)/scsi: $(OBJECTS) $(MAKE_BUILDDIR) $(DRIVERDEPS)
	$(DRIVERLINK) $(OBJECTS) $(DRIVERLIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 644 scsi.map $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers
#	$(INSTALL) -d $(EROS_ROOT)/lib/$(EROS_TARGET)
#	$(INSTALL) -m 0644 $(LIBTARGETS) $(EROS_ROOT)/lib/$(EROS_TARGET)

-include $(BUILDDIR)/.*.m
