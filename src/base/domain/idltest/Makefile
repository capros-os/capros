#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, Strawberry Development Group
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

default: package

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk

IDL=$(wildcard *.idl)
TARGETS=$(BUILDDIR)/idltest
OPTIM=-O
OBJECTS=$(BUILDDIR)/idltest.o
INC=-I$(EROS_ROOT)/include

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/idltest: $(OBJECTS) $(DOMCRT0) $(DOMLIB)
	$(CROSSLINK) $(OBJECTS) $(CROSSLIBS) -o $@

dotest: $(BUILDDIR)/idltest.c

$(BUILDDIR)/idltest.c: $(BUILDDIR) $(IDL)
	$(CAPIDL) -I$(EROS_ROOT)/idl -l c-server -o $@ $(IDL)

$(BUILDDIR)/idltest.o: $(BUILDDIR)/idltest.c
	$(C_BUILD)
	$(C_DEP)

interfaces: $(IDL)
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/domain
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/domain
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros/domain

# This is a temporary expedient so that the tree build proceeds
# unmolested:
install:

#install: all
#	$(INSTALL) -d $(CAPROS_DOMAIN)
#	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
