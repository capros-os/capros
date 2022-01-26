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

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk

VPATH=$(EROS_TARGET)
TARGETS=$(BUILDDIR)/protospace
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include

LIBTARGETS=$(BUILDDIR)/interpreter.o

HEADERS=InterpreterTable.h
DOMHEADERS=ProtoSpace.h ProtoSpaceDS.h InterpreterDestroy.h

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS) $(LIBTARGETS)

# Combine protospace.S and telospace.S into a single object file
# that will get loaded into a single page
# (just to save having another separate page). 
# telospace.S is loaded at the fixed address 0x400.
$(BUILDDIR)/protospace: $(BUILDDIR)/protospace1 $(BUILDDIR)/telospace
	$(TARGET_LD) -N -Ttext 0 --section-start .texttelo=0x400 --section-start .rodatatelo=0x500 -static -e _start $(BUILDDIR)/protospace1 $(BUILDDIR)/telospace -o $@
#	$(TARGET_OBJDUMP) -x $@ > $(BUILDDIR)/all.dump

$(BUILDDIR)/protospace1: $(BUILDDIR)/protospace.o
# Link without $(DOMLIB). Should not need any other procedures.
# Protospace is well-known to start at zero.
	$(TARGET_LD) -Ttext 0 -N $(BUILDDIR)/protospace.o -o $@

$(BUILDDIR)/telospace: $(BUILDDIR)/telospace.o
# Link without $(DOMLIB). Should not need any other procedures.
# Telospace is well-known to start at 0x400.
# I don't know how to get section .rodatatelo to follow .texttelo,
# so put it at a fixed address.
	$(TARGET_LD) --section-start .texttelo=0x400 --section-start .rodatatelo=0x500 -N $(BUILDDIR)/telospace.o -o $@
	$(TARGET_OBJCOPY) --keep-global-symbol=telospacestart --set-section-flags .texttelo=alloc,code,load --set-section-flags .rodatatelo=alloc,load,readonly,data $@

interfaces:
	$(INSTALL) -d $(EROS_ROOT)/include/domain
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/include
	$(INSTALL) -m 0644 $(DOMHEADERS) $(EROS_ROOT)/include/domain

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 0644 $(LIBTARGETS) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
