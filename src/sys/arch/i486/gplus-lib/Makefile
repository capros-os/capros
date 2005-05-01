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

EROS_SRC=../../../..
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk
BUILDDIR=.

GCC2OBJECTS=$(BUILDDIR)/gnu__umoddi3.o $(BUILDDIR)/gnu__udivdi3.o
LIBOBJECTS=$(BUILDDIR)/gnu__main.o $(BUILDDIR)/gnu__del.o $(GCC2OBJECTS)
OBJECTS=$(BUILDDIR)/gnu_crt1.o $(BUILDDIR)/gnu_crtn.o $(LIBOBJECTS)
DIRS=
TARGETS=$(BUILDDIR)/gplus-lib.a  $(BUILDDIR)/gnu_crt1.o $(BUILDDIR)/gnu_crtn.o
CLEANLIST=gplus-lib.a


include $(EROS_SRC)/build/make/makerules.mk
.SUFFIXES: .cxx .o .m

all: $(TARGETS)

install: all

$(BUILDDIR)/gnu__umoddi3.o: gnu_libgcc2.c
	$(GCC) -c -o $@ -DL_umoddi3 -DL_trampoline $<

$(BUILDDIR)/.gnu__umoddi3.m:
	@echo "$@: gnu_libgcc2.c" >> $@

$(BUILDDIR)/gnu__udivdi3.o: gnu_libgcc2.c
	$(GCC) -c -o $@ -DL_udivdi3 -DL_trampoline $<

$(BUILDDIR)/.gnu__udivdi3.m:
	@echo "$@: gnu_libgcc2.c" >> $@

$(BUILDDIR)/gplus-lib.a: $(LIBOBJECTS)
	$(AR) rcv gplus-lib.a $(LIBOBJECTS)
	$(RANLIB) gplus-lib.a

-include $(BUILDDIR)/.*.m
