#
# Copyright (C) 2008, 2009, Strawberry Development Group.
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

TARGETS=$(BUILDDIR)/ds2480b
ifeq "$(EROS_TARGET)" "i486"	# drivers for i486 only
else	# ARM-only drivers
DRIVER_TARGETS=$(BUILDDIR)/ds2490
endif

DS2490_OBJECTS=$(BUILDDIR)/ds2490.o $(BUILDDIR)/ds2490_cap.o

LIBS=../../usb/lib/$(BUILDDIR)/libusbdev.a

INC=-I$(EROS_ROOT)/include/linux-headers -I$(EROS_ROOT)/include
INC+= -I$(EROS_ROOT)/host/include
INC+= -include $(EROS_ROOT)/include/linuxk/linux-emul.h

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS) $(DRIVER_TARGETS) $(BUILDDIR)/ds2490_ids.o

$(BUILDDIR)/ds2490: $(DS2490_OBJECTS) $(LIBS) $(DYNDRIVERDEPS)
	$(DYNDRIVERLINK) $(DS2490_OBJECTS) $(LIBS) $(DRIVERLIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled2490

$(BUILDDIR)/ds2480b:  $(BUILDDIR)/ds2480b.o $(DOMLIB)
	$(CROSSLINK) $(BUILDDIR)/ds2480b.o $(SMALL_SPACE) $(CROSSLIBS) -o $@
# and for debugging:
#	$(EROS_OBJDUMP) -t --disassemble $@ >$(BUILDDIR)/disassembled2480b

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 644 ds2490.map $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 755 $(DRIVER_TARGETS) $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 644 ds2480b.map $(CAPROS_DOMAIN)
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
