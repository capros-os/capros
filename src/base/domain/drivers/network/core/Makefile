#
# Copyright (C) 2008, Strawberry Development Group.
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

default: install

CROSS_BUILD=yes

EROS_SRC=../../../../..
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include
INC+= -I$(EROS_ROOT)/include/linux-headers
INC+= -include $(EROS_ROOT)/include/linuxk/linux-emul.h
include $(EROS_SRC)/build/make/makevars.mk

OBJECTS=
OBJECTS+=$(BUILDDIR)/dev.o
OBJECTS+=$(BUILDDIR)/eth.o
OBJECTS+=$(BUILDDIR)/link_watch.o
OBJECTS+=$(BUILDDIR)/mii.o
OBJECTS+=$(BUILDDIR)/neighbour.o
OBJECTS+=$(BUILDDIR)/rtnetlink.o
OBJECTS+=$(BUILDDIR)/skbuff.o
OBJECTS+=$(BUILDDIR)/sock.o
OBJECTS+=$(BUILDDIR)/stubs.o
OBJECTS+=$(BUILDDIR)/utils.o

include $(EROS_SRC)/build/make/makerules.mk

all: $(OBJECTS)

install: all

-include $(BUILDDIR)/.*.m
