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

INC=-I$(EROS_ROOT)/include
OPTIM=-O
DIRS=
TARGETS=$(BUILDDIR)/libkeycache.a
OBJECTS=$(BUILDDIR)/keycache.o

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/keycache.o: keycache.c $(MAKE_BUILDDIR)
# Need to use -ansi option, so ffs will not be declared builtin. 
	$(GCC) $(GCCFLAGS) $(GCCWARN) -ansi -D$(EROS_TARGET) -c $< -o $@

$(TARGETS): $(OBJECTS)
	$(AR) -crv $(TARGETS) $(OBJECTS)
	$(RANLIB) $(TARGETS)

install: all
	$(INSTALL) -d $(EROS_ROOT)/lib
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/lib

-include $(BUILDDIR)/.*.m
