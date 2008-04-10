#
# Copyright (C) 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, Strawberry Development Group.
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
TARGETS=$(BUILDDIR)/_3c905c $(BUILDDIR)/helper 
HELPER=$(BUILDDIR)/helper.o
_3C905C=$(BUILDDIR)/_3c905c.o
IMGMAP=_3c905c.map

include $(EROS_SRC)/build/make/makerules.mk

OPTIM=-O

all: $(TARGETS)

_3c905c.c:$(BUILDDIR)/constituents.h
helper.c:$(BUILDDIR)/constituents.h

$(BUILDDIR)/_3c905c: $(_3C905C) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(_3C905C) $(CROSSLIBS) 
$(BUILDDIR)/helper: $(HELPER) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(HELPER) $(CROSSLIBS) 

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers/network/3com
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers/network/3com
	$(INSTALL) -m 644 _3c905c.map $(CAPROS_DOMAIN)/drivers/network/3com

-include $(BUILDDIR)/.*.m
