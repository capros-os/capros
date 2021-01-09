#
# Copyright (C) 2008, 2010, Strawberry Development Group
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

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk

IMGMAP=w1mult.map
HEADERS=w1multConfig.h
TARGETS=$(BUILDDIR)/w1mult
OBJECTS=$(BUILDDIR)/w1mult.o
OBJECTS+= $(BUILDDIR)/DS18B20.o
OBJECTS+= $(BUILDDIR)/DS2408.o
OBJECTS+= $(BUILDDIR)/DS2450.o
OBJECTS+= $(BUILDDIR)/DS2438.o
INC=-I$(BUILDDIR) -I$(CAPROS_HEADERS)/include

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

IDL=$(wildcard *.idl)
interfaces: $(IDL)
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros
	$(INSTALL) -d $(EROS_ROOT)/include
	$(INSTALL) -m 644 $(HEADERS) $(EROS_ROOT)/include

$(BUILDDIR)/w1mult: $(OBJECTS) $(DOMCRT0) $(DOMLIB)
	$(CROSSLINK) $(OBJECTS) $(SMALL_SPACE) $(CROSSLIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled
	$(EROS_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/syms

install: all
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 $(IMGMAP) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
