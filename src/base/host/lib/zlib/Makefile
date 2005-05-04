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
VPATH=$(KERNDIR)/zlib
INC=	-I$(EROS_ROOT)/host/include -I$(KERNDIR)/zlib
DEF= -DSTDC
OPTIM=-O
DIRS=
TARGETS=libzlib.a

KERNSRC=$(notdir $(wildcard $(KERNDIR)/zlib/*.c))

OBJECTS=$(addprefix $(BUILDDIR)/,$(KERNSRC:.c=.o))
HEADERS=$(notdir $(wildcard $(KERNDIR)/zlib/*.h))

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
	$(INSTALL) -m 0644 $(HEADERS:%=$(KERNDIR)/zlib/%) $(EROS_ROOT)/host/include

.PRECIOUS: $(HEADERS) $(KERNSRC)

-include $(BUILDDIR)/.*.m
