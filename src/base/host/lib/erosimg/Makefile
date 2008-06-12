#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2007, 2008, Strawberry Development Group.
#
# This file is part of the CapROS Operating System,
# and is derived from the EROS Operating System.
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

EROS_SRC=../../../..
include $(EROS_SRC)/build/make/makevars.mk

# This is not functional at the moment.
#SUPPORT_AOUT=-DSUPPORT_AOUT

INC=	-I../ -I$(EROS_ROOT)/host/include $(XENV_INCLUDE) -I$(BUILDDIR)

OPTIM=-g
DIRS=
TARGETS=$(BUILDDIR)/liberos.a
DEF += $(SUPPORT_AOUT)
DEF += -D_REVEAL_KERNEL_KEY_TYPES_

GENERATED = $(BUILDDIR)/gen.RegisterDescriptions.c
C_SOURCES= $(wildcard *.c)
CXX_SOURCES= $(wildcard *.cxx)
SOURCES= $(filter-out $(GENERATED),$(C_SOURCES) $(CXX_SOURCES))
RAW_OBJECTS= $(wildcard *.cxx) $(wildcard *.c)
OBJECTS= $(addprefix $(BUILDDIR)/,$(CXX_SOURCES:.cxx=.o) $(C_SOURCES:.c=.o))

HEADERS= $(wildcard *.hxx) $(wildcard *.h)

CLEANLIST += $(GENERATED)

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/liberos.a: $(OBJECTS) $(BUILDDIR)
	$(AR) -crv $@ $(OBJECTS)
	$(RANLIB) $@

install: all
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/lib
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/host/lib
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/include
	$(INSTALL) -d $(EROS_ROOT)/host/include/erosimg
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/host/include/erosimg

LAYOUT = $(EROS_SRC)/sys/arch/$(EROS_TARGET)/LAYOUT
GENDESCRIP = $(EROS_SRC)/sys/scripts/GenArchDescrip.awk
$(BUILDDIR)/gen.RegisterDescriptions.c: $(LAYOUT) $(GENDESCRIP) $(BUILDDIR)
	$(EROS_CPP) -I$(EROS_SRC)/sys $(LAYOUT) | $(GAWK) -f $(GENDESCRIP) > $@

ArchDescrip.c: $(BUILDDIR)/gen.RegisterDescriptions.c

-include $(BUILDDIR)/.*.m
