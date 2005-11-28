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


# format utility for cross-formatting a volume.  THIS IS NOT A NATIVE TOOL
default: package

EROS_SRC=../../../..

include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/lsvol $(BUILDDIR)/dbvol $(BUILDDIR)/setvol
OPTIM=-g
COMMON_OBJECTS=$(BUILDDIR)/prt_ckdir.o $(BUILDDIR)/prt_divtbl.o 
COMMON_OBJECTS+=$(BUILDDIR)/prt_volhdr.o $(BUILDDIR)/prt_rsdir.o
OTHER_OBJECTS=$(BUILDDIR)/lsvol.o $(BUILDDIR)/dbvol.o $(BUILDDIR)/setvol.o
OBJECTS=$(OTHER_OBJECTS) $(COMMON_OBJECTS)
DEF+=   -D_REVEAL_KERNEL_KEY_TYPES_
INC=	-I$(EROS_ROOT)/host/include $(XENV_INCLUDE)
LIBS=	$(EROS_ROOT)/host/lib/liberos.a $(EROS_ROOT)/host/lib/libdisk.a
DIRS=

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/lsvol: $(BUILDDIR)/lsvol.o $(COMMON_OBJECTS) $(LIBS)
	$(GPLUS) $(GPLUSFLAGS) -o $@ $(BUILDDIR)/lsvol.o $(COMMON_OBJECTS) $(LIBS)

$(BUILDDIR)/dbvol: $(BUILDDIR)/dbvol.o $(COMMON_OBJECTS) $(LIBS)
	$(GPLUS) $(GPLUSFLAGS) -o $@ $(BUILDDIR)/dbvol.o $(COMMON_OBJECTS) $(LIBS)

$(BUILDDIR)/setvol: $(BUILDDIR)/setvol.o $(COMMON_OBJECTS) $(LIBS)
	$(GPLUS) $(GPLUSFLAGS) -o $@ $(BUILDDIR)/setvol.o $(COMMON_OBJECTS) $(LIBS)

install: all
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/bin
	$(INSTALL) -m 755 $(TARGETS) $(EROS_ROOT)/host/bin

-include $(BUILDDIR)/.*.m
