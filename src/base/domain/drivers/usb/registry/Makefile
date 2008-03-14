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
EROS_SRC=../../../../..

include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/usbregistry
OBJECTS=$(BUILDDIR)/main.o

STORID_OBJECTS=../storage/$(BUILDDIR)/stor_ids.o
STORID_OBJECTS+=../../w1/masters/$(BUILDDIR)/ds2490_ids.o

INC=-I$(EROS_ROOT)/include/linux-headers -I$(EROS_ROOT)/include
INC+= -include $(EROS_ROOT)/include/linuxk/linux-emul.h

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

IDL=$(wildcard *.idl)
interfaces: $(IDL)
##	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros

$(BUILDDIR)/usbregistry: $(OBJECTS) $(DOMCRT0) $(DOMLIB)
	$(DOMLINK) $(DOMLINKOPT) $(DOMCRT0) $(OBJECTS) $(STORID_OBJECTS) $(DOMLIB) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 644 usbregistry.map $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers

-include $(BUILDDIR)/.*.m
