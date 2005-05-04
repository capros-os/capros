#
# Copyright (C) 2003, Jonathan S. Shapiro.
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
#
default: install

CROSS_BUILD=yes

EROS_SRC=../../..
EROS_CONFIG=NOKBD
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include

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

$(TARGETS): $(OBJECTS) $(DOMLIB) $(DOMCRT0)

$(LDMAIN):
	$(DOMLINK) $(DOMLINKOPT) -o $@ $(DOMCRT0) $(OBJECTS) $(LIBS) $(DOMLIB)

interfaces: $(IDL)
	$(INSTALL) -d $(EROS_ROOT)/idl/eros/domain
	$(INSTALL) -d $(EROS_ROOT)/idl/eros/domain/linedisc
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/eros/domain/linedisc

install: all
	$(INSTALL) -d $(EROS_ROOT)/domain/linedisc
	$(INSTALL) -d $(EROS_ROOT)/domain/linedisc
	$(INSTALL) -m 755 $(LDOUT) $(LDMAIN) $(EROS_ROOT)/domain/linedisc
	$(INSTALL) -m 644 $(IMGMAP) $(EROS_ROOT)/domain/linedisc

-include $(BUILDDIR)/.*.m
