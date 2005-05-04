#
# Copyright (C) 2001, Jonathan S. Shapiro.
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
default: all

CROSS_BUILD=yes

EROS_SRC=../../../../..
EROS_CONFIG=DEFAULT
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include
include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
TARGETS=$(BUILDDIR)/tg3 $(BUILDDIR)/helper 
HELPER=$(BUILDDIR)/helper.o
TG3=$(BUILDDIR)/tg3.o
IMGMAP=tg3.map

include $(EROS_SRC)/build/make/makerules.mk

OPTIM=-O

all: $(TARGETS)

tg3.c:$(BUILDDIR)/constituents.h
helper.c:$(BUILDDIR)/constituents.h

$(BUILDDIR)/tg3: $(TG3) $(DOMLIB) $(DOMCRT0)
	$(DOMLINK) $(DOMLINKOPT) -o $@ $(DOMCRT0) $(TG3) $(DOMLIB) 
$(BUILDDIR)/helper: $(HELPER) $(DOMLIB) $(DOMCRT0)
	$(DOMLINK) $(DOMLINKOPT) -o $@ $(DOMCRT0) $(HELPER) $(DOMLIB) 

install: all
	$(INSTALL) -d $(EROS_ROOT)/domain/drivers/network/tg3
	$(INSTALL) -m 755 $(TARGETS) $(EROS_ROOT)/domain/drivers/network/tg3
	$(INSTALL) -m 644 tg3.map $(EROS_ROOT)/domain/drivers/network/tg3

-include $(BUILDDIR)/.*.m
