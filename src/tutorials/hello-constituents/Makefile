#
# Copyright (C) 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, Strawberry Development Group
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

CROSS_BUILD=yes

EROS_SRC=../..
include $(EROS_SRC)/build/make/makevars.mk

#GENERATED=$(BUILDDIR)/constituents.h
TARGETS=$(BUILDDIR)/hello
OBJECTS=$(BUILDDIR)/hello.o

include $(EROS_SRC)/build/make/makerules.mk
include ../tutorial.mk

#install all: $(TARGETS)

hello.c: $(BUILDDIR)/constituents.h

$(BUILDDIR)/hello: $(OBJECTS) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(OBJECTS) $(SMALL_SPACE) $(CROSSLIBS)

-include $(BUILDDIR)/.*.m
