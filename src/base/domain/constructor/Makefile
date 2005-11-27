#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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


default: package

EROS_SRC=../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk


GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=metacon.map
VPATH=$(EROS_TARGET)
TARGETS=$(BUILDDIR)/constructor $(BUILDDIR)/protospace $(BUILDDIR)/metaconstructor
OPTIM=-O
OBJECTS=$(BUILDDIR)/constructor.o $(BUILDDIR)/protospace.o $(BUILDDIR)/metaconstructor.o
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

$(BUILDDIR)/protospace: $(BUILDDIR)/protospace.o $(DOMCRT0) $(DOMLIB)
	$(DOMLINK) $(DOMLINKOPT) $(BUILDDIR)/protospace.o $(DOMLIB) -o $@
#	objdump --headers protospace

install: all
	$(INSTALL) -d $(EROS_ROOT)/domain
	$(INSTALL) -m 644 metacon.map $(EROS_ROOT)/domain
	$(INSTALL) -m 755 $(TARGETS) $(EROS_ROOT)/domain

-include $(BUILDDIR)/.*.m
