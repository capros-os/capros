#
# Copyright (C) 2003, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, Strawberry Development Group
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
IMGMAP=eterm.map
ANSI=$(BUILDDIR)/eterm_ansi
ANSIOBJ=$(ANSI).o
EMAIN=$(BUILDDIR)/eterm_main
EMAINOBJ=$(EMAIN).o
EOUT=$(BUILDDIR)/eterm_out
EOUTOBJ=$(EOUT).o
VPATH=$(EROS_TARGET)
TARGETS=idlstuff $(EMAIN) $(EOUT)
SOURCES=$(wildcard *.c)

OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(SOURCES))

LIBS=-laddrspace -lgraphics

include $(EROS_SRC)/build/make/makerules.mk

INC+=-I.
OPTIM=-O

all: $(TARGETS)

$(BUILDDIR)/%.o: $(BUILDDIR)/%.c
	$(C_BUILD)

$(SOURCES): $(BUILDDIR)/constituents.h

$(TARGETS): $(OBJECTS) $(DOMLIB) $(DOMCRT0)

$(EMAIN):
	$(DOMLINK) $(DOMLINKOPT) -o $@ $(DOMCRT0) $(EMAINOBJ) $(LIBS) $(DOMLIB)

$(EOUT):
	$(DOMLINK) $(DOMLINKOPT) -o $@ $(DOMCRT0) $(ANSIOBJ) $(EOUTOBJ) $(LIBS) $(DOMLIB)

interfaces: $(IDL)
	$(INSTALL) -d $(EROS_ROOT)/idl/eros/domain
	$(INSTALL) -d $(EROS_ROOT)/idl/eros/domain/eterm
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/eros/domain/eterm

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/eterm
	$(INSTALL) -m 755 $(EOUT) $(EMAIN) $(CAPROS_DOMAIN)/eterm
	$(INSTALL) -m 644 $(IMGMAP) $(CAPROS_DOMAIN)/eterm

-include $(BUILDDIR)/.*.m
