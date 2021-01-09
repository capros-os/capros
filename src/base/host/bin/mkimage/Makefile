#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, 2011, Strawberry Development Group.
#
# This file is part of the CapROS Operating System,
# and is derived from the EROS Operating System.
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


# Utility to convert a linux a.out file to a domain image file.
default: package

EROS_SRC=../../../..
include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/$(EROS_TARGET)-mkimage
#OPTIM=-O
OPTIM=-g
# Use MYOBJECTS, because automatic dependency generation doesn't work
# for this.
OBJECTS= $(BUILDDIR)/gram.o $(BUILDDIR)/lex.o $(BUILDDIR)/PtrMap.o
DEF+=   -D_REVEAL_KERNEL_KEY_TYPES_
INC=	-I$(BUILDDIR) -I. -I$(EROS_ROOT)/host/include $(XENV_INCLUDE)
LIBS=	$(EROS_ROOT)/host/lib/liberos.a $(EROS_ROOT)/host/lib/libdisk.a
OTHER_LIBS=	-lbfd -liberty -ldl -lz
DIRS=
HEADERS=mkimage.preinclude

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/$(EROS_TARGET)-mkimage: $(OBJECTS) $(LIBS)
	$(GPLUS) $(GPLUSFLAGS) -o $@ $(OBJECTS) $(XENV_LIBDIR) $(LIBS) $(OTHER_LIBS)

$(BUILDDIR)/gram.o: $(BUILDDIR)/gram.c $(MAKE_BUILDDIR)
	$(C_BUILD) 
	$(C_DEP)

$(BUILDDIR)/lex.o: $(BUILDDIR)/lex.c $(BUILDDIR)/gram.c.h $(MAKE_BUILDDIR)
# Can't use $(C_BUILD) because it has -Werror and lex.c generates warnings.
	$(GCC) $(GCCFLAGS) $(MOPSWARN) -c $< -o $@
	$(C_DEP)

.NOTPARALLEL:
# the bison rule below is not re-entrant.

$(BUILDDIR)/gram.c $(BUILDDIR)/gram.c.h: gram.y $(MAKE_BUILDDIR)
	(cd $(BUILDDIR); bison -v -d -y ../../gram.y)
	(cd $(BUILDDIR); mv y.tab.c gram.c)
	(cd $(BUILDDIR); mv y.tab.h gram.c.h)
	#bison -v -d -o$@ gram.y

$(BUILDDIR)/lex.c: lex.l $(BUILDDIR)/gram.c.h $(MAKE_BUILDDIR)
	flex -d -o$@ lex.l

install: all
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/bin
	$(INSTALL) -d $(EROS_ROOT)/host/include
	$(INSTALL) -m 755 $(TARGETS) $(EROS_ROOT)/host/bin
	$(INSTALL) -m 755 $(HEADERS) $(EROS_ROOT)/host/include

-include $(BUILDDIR)/.*.m
