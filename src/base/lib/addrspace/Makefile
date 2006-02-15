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

default: install

EROS_SRC=../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

HEADERS=$(wildcard *.h)
OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(wildcard *.c))
TARGETS=$(BUILDDIR)/libaddrspace.a

INC=-I.. -I$(EROS_ROOT)/include
OPTIM=-O

include $(EROS_SRC)/build/make/makerules.mk

all install: $(TARGETS)

$(TARGETS): $(OBJECTS)
	$(AR) -crv $@ $(OBJECTS)
	$(RANLIB) $@

install:
	$(INSTALL) -d $(EROS_ROOT)/lib
	$(INSTALL) -d $(EROS_ROOT)/lib/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/lib/$(EROS_TARGET)
	$(INSTALL) -d $(EROS_ROOT)/include
	$(INSTALL) -d $(EROS_ROOT)/include/addrspace
	$(INSTALL) -m 644 $(HEADERS) $(EROS_ROOT)/include/addrspace

-include $(BUILDDIR)/.*.m
