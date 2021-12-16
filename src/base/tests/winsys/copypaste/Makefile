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
default: all

CROSS_BUILD=yes

EROS_SRC=../../../..
EROS_CONFIG=NOKBD
include $(EROS_SRC)/build/make/makevars.mk

DEMO_FILE=copy.txt
SOURCES=$(wildcard *.c)
GENERATED=$(BUILDDIR)/constituents.h
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(SOURCES)))
TARGETS=$(BUILDDIR)/master $(BUILDDIR)/copier $(BUILDDIR)/paster

LIBS=-laddrspace -lgraphics

include $(EROS_SRC)/build/make/makerules.mk
include $(EROS_SRC)/base/tests/drivers/drivers.mk

OPTIM=-O

all: $(TARGETS)

$(TARGETS): $(BUILDDIR)/$(DEMO_FILE)

$(BUILDDIR)/$(DEMO_FILE):
	cp $(DEMO_FILE) $(BUILDDIR)/$(DEMO_FILE)

$(SOURCES): $(BUILDDIR)/constituents.h

$(BUILDDIR)/master: $(OBJECTS) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $@.o $(LIBS) $(CROSSLIBS)

$(BUILDDIR)/copier: $(OBJECTS) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $@.o $(LIBS) $(CROSSLIBS)

$(BUILDDIR)/paster: $(OBJECTS) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $@.o $(LIBS) $(CROSSLIBS)


-include $(BUILDDIR)/.*.m
