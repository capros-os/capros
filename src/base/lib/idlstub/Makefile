#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2007, 2008, Strawberry Development Group.
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

EROS_SRC=../../..
CROSS_BUILD=yes
CLEANLIST=$(wildcard $(IDL_STUB_DIR)/*.c)

include $(EROS_SRC)/build/make/makevars.mk

VPATH=$(BUILDDIR):arch/$(EROS_TARGET)
TARGETS=$(BUILDDIR)/libidlstub.a
# Generated stubs are in $(BUILDDIR)/*.c
OBJECTS=$(patsubst %.c,%.o,$(wildcard $(BUILDDIR)/*.c))
# Client (handwritten) stubs are in *.c
OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard *.c))
# Assembler client stubs are in arch/$(EROS_TARGET)/*.S
OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(wildcard arch/$(EROS_TARGET)/*.S)))
INC= -I$(EROS_ROOT)/include -I$(EROS_ROOT)/include/linux-headers

$(BUILDDIR)/libidlstub.a: $(OBJECTS)
	echo $(OBJECTS)
	$(RM) $@
	$(AR) -cr $@ $(OBJECTS)
	$(RANLIB) $@

all:
	$(MAKE) $(MAKERULES) $(TARGETS)

install: all
	$(INSTALL) -d $(EROS_ROOT)/lib
	$(INSTALL) -d $(EROS_ROOT)/lib/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/lib/$(EROS_TARGET)

include $(EROS_SRC)/build/make/makerules.mk
