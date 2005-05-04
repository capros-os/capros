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
include $(EROS_SRC)/build/make/makevars.mk

KERNDIR=$(EROS_SRC)/sys
VPATH=$(KERNDIR)/disk
INC=	-I$(EROS_ROOT)/include -I$(KERNDIR) # -I$(EROS_ROOT)/host/include
OPTIM=-g
DIRS=
TARGETS=$(BUILDDIR)/libdisk.a

CXX_KERNSRC=	$(notdir $(wildcard $(KERNDIR)/disk/*.cxx))
C_KERNSRC=	$(notdir $(wildcard $(KERNDIR)/disk/*.c))

CXX_OBJECTS=        $(addprefix $(BUILDDIR)/,$(CXX_KERNSRC:.cxx=.o))
C_OBJECTS=        $(addprefix $(BUILDDIR)/,$(C_KERNSRC:.c=.o))

OBJECTS= $(C_OBJECTS) $(CXX_OBJECTS)

HEADERS=	$(notdir $(wildcard $(KERNDIR)/disk/*.hxx))
HEADERS+=	$(notdir $(wildcard $(KERNDIR)/disk/*.h))
HEADERS+=	$(notdir $(wildcard $(KERNDIR)/disk/*.def))

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(TARGETS): $(OBJECTS)
	$(AR) -crv $(TARGETS) $(OBJECTS)
	$(RANLIB) $(TARGETS)

install: all
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/lib
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/host/lib/
	$(INSTALL) -d $(EROS_ROOT)/host/include
	$(INSTALL) -d $(EROS_ROOT)/host/include/disk
	$(INSTALL) -m 0644 $(HEADERS:%=$(KERNDIR)/disk/%) $(EROS_ROOT)/host/include/disk

.PRECIOUS: $(HEADERS) $(KERNSRC)

-include $(BUILDDIR)/.*.m
