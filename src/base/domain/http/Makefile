#
# Copyright (C) 2009, 2011, Strawberry Development Group
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

COMPONENT=http
EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/$(COMPONENT)
OBJECTS=$(BUILDDIR)/$(COMPONENT).o
OBJECTS+=$(BUILDDIR)/hrh.o
OBJECTS+=$(BUILDDIR)/hfile.o
OBJECTS+=$(BUILDDIR)/stubs.o

TARGETS+=$(BUILDDIR)/httprget
TARGETS+=$(BUILDDIR)/httprpost

INC=-I$(BUILDDIR) -I$(CAPROS_HEADERS)/include -I../openssl/include
IMGMAP=$(COMPONENT).map
LIBS=-lssl -lcrypto
LIBDEP+=$(CAPROS_DOMAIN)/libssl.a
LIBDEP+=$(CAPROS_DOMAIN)/libcrypto.a

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

IDL=$(wildcard *.idl)
interfaces: $(IDL)
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros

$(BUILDDIR)/$(COMPONENT): $(OBJECTS) $(LIBDEP) $(DYNCMMEDEPS)
	$(DYNCMMELINK) $(OBJECTS) $(LIBS) $(CMMELIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/syms
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

$(BUILDDIR)/httprget: $(BUILDDIR)/httprget.o $(LIBDEP)
	$(CROSSLINK) $(BUILDDIR)/httprget.o $(SMALL_SPACE) $(CROSSLIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/symshttpr
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembledhttpr

$(BUILDDIR)/httprpost: $(BUILDDIR)/httprpost.o $(LIBDEP)
	$(CROSSLINK) $(BUILDDIR)/httprpost.o $(SMALL_SPACE) $(CROSSLIBS) -o $@
# and for debugging:
	$(EROS_OBJDUMP) --syms $@ | sort >$(BUILDDIR)/symshttprpost
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembledhttprpost

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 $(IMGMAP) httprget.map httprpost.map $(CAPROS_DOMAIN)
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
