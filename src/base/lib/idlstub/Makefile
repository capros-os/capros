#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2007, Strawberry Development Group.
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
CLEANLIST=$(wildcard *.c)

include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/libidlstub.a
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard *.c))
INC= -I$(EROS_ROOT)/include

$(BUILDDIR)/libidlstub.a: $(OBJECTS)
	$(AR) -crv $@ $(OBJECTS)
	$(RANLIB) $@

all:
	$(MAKE) $(MAKERULES) $(TARGETS)

install: all
	$(INSTALL) -d $(EROS_ROOT)/lib
	$(INSTALL) -d $(EROS_ROOT)/lib/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/lib/$(EROS_TARGET)

include $(EROS_SRC)/build/make/makerules.mk
