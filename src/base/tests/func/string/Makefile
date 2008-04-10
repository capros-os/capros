#
# Copyright (C) 2005, Strawberry Development Group
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
#


default: install

CROSS_BUILD=yes

EROS_SRC=../../../..
include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/call $(BUILDDIR)/echo
OPTIM=-O
OBJECTS=$(BUILDDIR)/call.o $(BUILDDIR)/echo.o
INC=-I$(EROS_ROOT)/include

BOOT=$(EROS_ROOT)/lib/$(EROS_TARGET)/image/boot
VOLMAP=../test.volmap

include ../test.mk

include $(EROS_SRC)/build/make/makerules.mk

$(BUILDDIR)/call: $(BUILDDIR)/call.o $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(BUILDDIR)/call.o $(CROSSLIBS)

$(BUILDDIR)/echo: $(BUILDDIR)/echo.o $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(BUILDDIR)/echo.o $(CROSSLIBS)

-include $(BUILDDIR)/.*.m
