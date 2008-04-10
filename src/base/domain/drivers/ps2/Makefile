#
# Copyright (C) 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, Strawberry Development Group.
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

EROS_SRC=../../../..
EROS_CONFIG=NOKBD
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include

include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=ps2.map
TARGETS =$(BUILDDIR)/helper12 $(BUILDDIR)/helper1 $(BUILDDIR)/ps2reader
HELPER12=$(BUILDDIR)/helper12.o 
HELPER1=$(BUILDDIR)/helper1.o
HELPERS=$(HELPER12) $(HELPER1)
PS2READER=$(BUILDDIR)/ps2reader.o
OBJECTS=$(PS2READER) $(HELPERS)

include $(EROS_SRC)/build/make/makerules.mk

OPTIM=-g #override!

all: $(TARGETS)

#$(OBJECTS:.o=.c): $(BUILDDIR)/constituents.h
helper12.c: $(BUILDDIR)/constituents.h
helper1.c:$(BUILDDIR)/constituents.h
ps2reader.c:$(BUILDDIR)/constituents.h


$(BUILDDIR)/helper12: $(HELPER12) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(HELPER12) $(CROSSLIBS)

$(BUILDDIR)/helper1: $(HELPER1) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(HELPER1) $(CROSSLIBS)

$(BUILDDIR)/ps2reader: $(PS2READER) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(PS2READER) $(CROSSLIBS)

$(BUILDDIR)/mouseclient: $(MOUSECLIENT) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(MOUSECLIENT) $(CROSSLIBS)

$(BUILDDIR)/keyclient: $(KEYCLIENT) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(KEYCLIENT) $(CROSSLIBS)

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers/ps2
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers/ps2
	$(INSTALL) -m 644 ps2.map $(CAPROS_DOMAIN)/drivers/ps2
-include $(BUILDDIR)/.*.m
