#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group
#
# This file is part of the CapROS Operating System,
# and is derived from the EROS Operating System.
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

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=pcc.map
TARGETS=$(BUILDDIR)/proccre $(BUILDDIR)/pcc
OPTIM=-O
OBJECTS=$(BUILDDIR)/proccre.o $(BUILDDIR)/pcc.o
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include -I$(EROS_ROOT)/host/include

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/proccre: $(BUILDDIR)/proccre.o $(DOMCRT0) $(DOMLIB)
	$(CROSSLINK) $(BUILDDIR)/proccre.o $(SMALL_SPACE) $(CROSSLIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/syms
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

$(BUILDDIR)/pcc: $(BUILDDIR)/pcc.o $(DOMCRT0) $(DOMLIB)
	$(CROSSLINK) $(BUILDDIR)/pcc.o $(SMALL_SPACE) $(CROSSLIBS) -o $@

pcc.c: $(BUILDDIR)/constituents.h
proccre.c: $(BUILDDIR)/constituents.h

IDL=$(wildcard *.idl)
interfaces: $(IDL)
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 pcc.map $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
