#
# Copyright (C) 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, 2007, Strawberry Development Group
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

default: all

CROSS_BUILD=yes

EROS_SRC=../../..
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include

include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=winsys.map

TARGETS=$(BUILDDIR)/winsys

# Purely for reasons of mental organization, JV has organized the
# winsys sources into various subdirectories. Use wildcards and
# VPATH to get them from their subdirectory locations.
#
# Note that the fbmgr/keeper/ subdirectory is a completely separate beast,
# so we cannot just wildcard */*.c here.
IDL=$(wildcard *.idl)
SOURCES=$(wildcard *.c)
SOURCES+=$(wildcard sessionmgr/*.c)
SOURCES+=$(wildcard fbmgr/*.c)
SOURCES+=$(wildcard fbmgr/drivers/*.c)
SOURCES+=$(wildcard fbmgr/drivers/bochs/*.c)
SOURCES+=$(wildcard fbmgr/drivers/vmware/*.c)
SOURCES+=$(wildcard keytrans/*.c)
ASM_SOURCES+=$(wildcard fbmgr/$(EROS_TARGET)/*.S)
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(SOURCES)))
OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(ASM_SOURCES)))
CURSORS=$(wildcard cursors/*.h)
VPATH=keytrans:sessionmgr:fbmgr:fbmgr/drivers:fbmgr/drivers/bochs:fbmgr/drivers/vmware:fbmgr/$(EROS_TARGET)

DIRS=keeper eventmgr 

LIBS=-lforwarder -lgraphics -laddrspace -lprintf

include $(EROS_SRC)/build/make/makerules.mk

INC+=-I.

OPTIM=-O

all: $(TARGETS)

interfaces: $(IDL)
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/domain
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/winsys
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros/winsys

winsys.c: $(BUILDDIR)/constituents.h $(CURSORS) debug.h

$(BUILDDIR)/winsys: $(OBJECTS) $(DOMLIB) $(DOMCRT0)
	$(DOMLINK) $(DOMLINKOPT) -o $@ $(DOMCRT0) $(OBJECTS) $(SUBDIROBJS) \
             $(FBMOBJS) $(VMWAREOBJS) $(LIBS) $(DOMLIB)

install: $(TARGETS)
	$(INSTALL) -d $(CAPROS_DOMAIN)/winsys
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/winsys
	$(INSTALL) -m 644 winsys.map $(CAPROS_DOMAIN)/winsys

-include $(BUILDDIR)/.*.m
