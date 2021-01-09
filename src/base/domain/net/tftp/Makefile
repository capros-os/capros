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

EROS_SRC=../../../..
INC=-I$(BUILDDIR) -I$(CAPROS_HEADERS)/include

include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=tftp.map
TARGETS=$(BUILDDIR)/tftp
TFTP=$(BUILDDIR)/tftp.o 

include $(EROS_SRC)/build/make/makerules.mk

OPTIM=-O

all: $(TARGETS)

tftp.c:	$(BUILDDIR)/constituents.h

$(BUILDDIR)/tftp: $(TFTP) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(TFTP) $(CROSSLIBS) 

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/net/tftp
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/net/tftp
	$(INSTALL) -m 644 tftp.map $(CAPROS_DOMAIN)/net/tftp


-include $(BUILDDIR)/.*.m
