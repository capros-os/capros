#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, Strawberry Development Group.
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

# This material is based upon work supported by the US Defense Advanced
# Research Projects Agency under Contract No. W31P4Q-07-C-0070.
# Approved for public release, distribution unlimited.


default: package

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk


GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=metacon.map
VPATH=$(EROS_TARGET)
TARGETS=$(BUILDDIR)/constructor $(BUILDDIR)/metaconstructor
TARGETS+= $(BUILDDIR)/protospace
OPTIM=-O
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/constructor: $(BUILDDIR)/constructor.o $(DOMCRT0) $(DOMLIB)
	$(DOMLINK) $(DOMLINKOPT) $(DOMCRT0) $(BUILDDIR)/constructor.o -lsmall $(DOMLIB) -o $@
#	objdump --headers constructor

metaconstructor.c: $(BUILDDIR)/constituents.h
constructor.c: $(BUILDDIR)/constituents.h

$(BUILDDIR)/metaconstructor: $(BUILDDIR)/metaconstructor.o $(DOMCRT0) $(DOMLIB)
	$(DOMLINK) $(DOMLINKOPT) $(DOMCRT0) $(BUILDDIR)/metaconstructor.o -lsmall $(DOMLIB) -o $@
#	objdump --headers metaconstructor

# Combine protospace.S and telospace.S into a single object file
# that will get loaded into a single page
# (just to save having another separate page). 
# telospace.S is loaded at the fixed address 0x400.
$(BUILDDIR)/protospace: $(BUILDDIR)/protospace1 $(BUILDDIR)/telospace
	$(DOMLINK) -N -Ttext 0 --section-start .texttelo=0x400 --section-start .rodatatelo=0x500 -static -e _start $(BUILDDIR)/protospace1 $(BUILDDIR)/telospace -o $@
#	$(EROS_OBJDUMP) -x $@ > $(BUILDDIR)/all.dump

$(BUILDDIR)/protospace1: $(BUILDDIR)/protospace.o
# Link without $(DOMLIB). Should not need any other procedures.
# Protospace is well-known to start at zero, so override DOMLINKOPT.
	$(DOMLINK) -Ttext 0 $(DOMLINKOPT) $(BUILDDIR)/protospace.o -o $@

$(BUILDDIR)/telospace: $(BUILDDIR)/telospace.o
# Link without $(DOMLIB). Should not need any other procedures.
# Telospace is well-known to start at 0x400, so override DOMLINKOPT.
# I don't know how to get section .rodatatelo to follow .texttelo,
# so put it at a fixed address.
	$(DOMLINK) --section-start .texttelo=0x400 --section-start .rodatatelo=0x500 $(DOMLINKOPT) $(BUILDDIR)/telospace.o -o $@
	$(EROS_OBJCOPY) --keep-global-symbol=telospacestart --set-section-flags .texttelo=alloc,code,load --set-section-flags .rodatatelo=alloc,load,readonly,data $@

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 metacon.map $(CAPROS_DOMAIN)
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
