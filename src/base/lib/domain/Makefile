#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, 2007, 2008, Strawberry Development Group.
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

# following is temporary, to help people clean up...
CLEANLIST=$(wildcard */*.[oa]) $(wildcard */*/*.[oa])

EROS_SRC=../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

KERNDIR=$(EROS_SRC)/sys/libkey
CAPSTUBDIR=$(EROS_SRC)/sys/arch/$(EROS_TARGET)/capstubs
VPATH=$(KERNDIR):$(CAPSTUBDIR):domdbg:key:crt/$(EROS_TARGET):crt:monitor:
INC=-Iinclude -I$(EROS_ROOT)/include
OPTIM=-O
DIRS=
# DIRS=include/domain/drivers not used

TARGETS=$(BUILDDIR)/libc-capros.a
TARGETS+=$(BUILDDIR)/libcapros-large.a
TARGETS+=$(BUILDDIR)/libcapros-small.a

# KERNSRC=$(notdir $(wildcard $(KERNDIR)/*.c)) not used
# DBGSRC=$(notdir $(wildcard domdbg/*.c))
# CRTSRC=$(notdir $(wildcard crt/libdom*.c)) not used
# MONSRC=$(notdir $(wildcard monitor/*.c)) not used

C_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/c_*.c)))

SMALL_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/small_*.c)))
SMALL_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/$(EROS_TARGET)/small_*.c)))
SMALL_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/$(EROS_TARGET)/small_*.S)))

LARGE_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/large_*.c)))
LARGE_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/$(EROS_TARGET)/large_*.c)))
LARGE_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/$(EROS_TARGET)/large_*.S)))

# Objects for both large and small:
CAPROS_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/capros_*.c)))
CAPROS_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/$(EROS_TARGET)/capros_*.c)))
CAPROS_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(wildcard crt/$(EROS_TARGET)/capros_*.S)))
CAPROS_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(notdir $(wildcard $(CAPSTUBDIR)/*.S)))
# Selected files from domdbg:
CAPROS_OBJECTS+=$(BUILDDIR)/kprintf.o
CAPROS_OBJECTS+=$(BUILDDIR)/wrstream.o
CAPROS_OBJECTS+=$(BUILDDIR)/sbrk.o
# Selected files from key:
# (Other stuff in key/ is not used and should be converted to IDL.)
CAPROS_OBJECTS+=$(BUILDDIR)/protospace.o
# keyset stuff should be converted to IDL, but until then:
CAPROS_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard key/keyset*.c)))
# nfile stuff should be converted to IDL, but until then:
CAPROS_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(wildcard key/nfile*.c)))
# KEYSRC=$(notdir $(wildcard key/*.c))
# CAPROS_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(KEYSRC))
CAPROS_OBJECTS+=$(wildcard ../idlstub/$(BUILDDIR)/*.o)

HEADERS=$(wildcard include/*/*.h) 

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/libc-capros.a: $(C_OBJECTS)
	$(AR) -crv $@ $(C_OBJECTS)

$(BUILDDIR)/libcapros-large.a: $(CAPROS_OBJECTS) $(LARGE_OBJECTS)
	$(AR) -crv $@ $(CAPROS_OBJECTS) $(LARGE_OBJECTS)

$(BUILDDIR)/libcapros-small.a: $(CAPROS_OBJECTS) $(SMALL_OBJECTS)
	$(AR) -crv $@ $(CAPROS_OBJECTS) $(SMALL_OBJECTS)

interfaces:
	$(INSTALL) -d $(EROS_ROOT)/include/domain
	$(INSTALL) -m 644 $(HEADERS) $(EROS_ROOT)/include/domain

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 0644 $(TARGETS) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
