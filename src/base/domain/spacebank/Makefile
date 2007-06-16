#
# Copyright (C) 1998, 1999, Jonathan Adams.
# Copyright (C) 2001, Jonathan S. Shapiro
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

default: package

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk

ETAGDIRS=. ../../sys/libkey ../../lib/domain/key
GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=primebank.map
TARGETS=$(BUILDDIR)/spacebank
CLEANLIST=$(BUILDDIR)/spacebank
OPTIM=-O2
SOURCES=spacebank.c	\
	AllocTree.c	\
	Bank.c		\
	ObjSpace.c	\
	assert.c	\
	heap.c		\
	malloc.c

OBJECTS=$(addprefix $(BUILDDIR)/,$(SOURCES:.c=.o))

INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include
INC += -I$(EROS_ROOT)/host/include # for DiskNodeStruct.h and ErosTypes.h
# DEFS possibilities include -DNDEBUG, -DPARANIOD
DEFS= -DNDEBUG 

ifeq "$(EROS_TARGET)" "i486" # only for i486 target
ASM_SOURCES=rdtsc.S
OBJECTS+=$(addprefix $(BUILDDIR)/,$(ASM_SOURCES:.S=.o))
# DEFS possibilities include -DTIMESTAMPS
endif

CFLAGS= -Wmissing-prototypes $(DEFS)

include $(EROS_SRC)/build/make/makerules.mk

DDIR=$(EROS_ROOT)/include/disk

all: $(TARGETS)

$(SOURCES): $(BUILDDIR)/constituents.h

$(BUILDDIR)/spacebank: $(OBJECTS) $(DOMCRT0) $(DOMLIB)
	$(DOMLINK) $(DOMLINKOPT) $(DOMCRT0) $(OBJECTS) $(DOMLIB) -o $@

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 primebank.map $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m

