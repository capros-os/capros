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
default: all

CROSS_BUILD=yes

EROS_SRC=../../../../..
EROS_CONFIG=NOKBD
INC=-I$(BUILDDIR) -I$(CAPROS_HEADERS)/include

include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=vmware_svga.map
VPATH=$(EROS_TARGET)
TARGETS=$(BUILDDIR)/vmware_svga
SOURCES=$(wildcard *.c)
LOCALHDRS=$(wildcard *.h)

LIBS=-lgraphics -laddrspace
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(SOURCES))

include $(EROS_SRC)/build/make/makerules.mk

OPTIM=-g #override!

all: $(TARGETS)

$(OBJECTS):	$(BUILDDIR)/constituents.h $(LOCALHDRS)

$(BUILDDIR)/vmware_svga: $(OBJECTS) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(OBJECTS) $(LIBS) $(CROSSLIBS)


install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers/pci
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers/pci
	$(INSTALL) -m 644 vmware_svga.map $(CAPROS_DOMAIN)/drivers/pci

-include $(BUILDDIR)/.*.m\
