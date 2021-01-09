#
# Copyright (C) 2007, 2008, 2009, Strawberry Development Group
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

EROS_SRC=../../../..
include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/test
OPTIM=-O
OBJECTS=$(BUILDDIR)/test.o
INC=-I$(CAPROS_HEADERS)/include

NPRANGESIZE=350
PRANGESIZE=17000

include $(EROS_SRC)/build/make/makerules.mk

install all: $(TARGETS)

include ../../test.mk

$(BUILDDIR)/test: $(BUILDDIR)/test.o $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(BUILDDIR)/test.o $(CROSSLIBS)
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

-include $(BUILDDIR)/.*.m
