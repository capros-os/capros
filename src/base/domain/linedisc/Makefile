#
# Copyright (C) 2003, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, 2008, Strawberry Development Group.
#
# This file is part of the EROS Operating System.
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

EROS_SRC=../../..
EROS_CONFIG=NOKBD
INC=-I$(BUILDDIR) -I$(CAPROS_HEADERS)/include

include $(EROS_SRC)/build/make/makevars.mk

IDL=$(wildcard *.idl)
GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=linedisc.map
LDMAIN=$(BUILDDIR)/linedisc
VPATH=$(EROS_TARGET)
TARGETS=$(LDMAIN)
SOURCES=$(wildcard *.c)

OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(SOURCES))

LIBS=-lethread

include $(EROS_SRC)/build/make/makerules.mk

INC+=-I.
OPTIM=-O

all: $(TARGETS)

$(BUILDDIR)/%.o: $(BUILDDIR)/%.c
	$(C_BUILD)

$(SOURCES): $(BUILDDIR)/constituents.h

$(LDMAIN): $(OBJECTS) $(DOMLIB)
	$(CROSSLINK) -o $@ $(OBJECTS) $(LIBS) $(CROSSLIBS)

interfaces: $(IDL)
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/domain
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/linedisc
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros/linedisc

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/linedisc
	$(INSTALL) -m 755 $(LDOUT) $(LDMAIN) $(CAPROS_DOMAIN)/linedisc
	$(INSTALL) -m 644 $(IMGMAP) $(CAPROS_DOMAIN)/linedisc

-include $(BUILDDIR)/.*.m
