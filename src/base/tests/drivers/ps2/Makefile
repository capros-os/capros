#
# Copyright (C) 2001, Jonathan S. Shapiro.
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

EROS_SRC=../../../..
EROS_CONFIG=NOKBD
include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
TARGETS =$(BUILDDIR)/ps2test $(BUILDDIR)/textconsole\
	$(BUILDDIR)/keyclient $(BUILDDIR)/mouseclient
PS2TEST=$(BUILDDIR)/ps2test.o
TEXTCON=$(BUILDDIR)/textconsole.o
MOUSECLIENT=$(BUILDDIR)/mouseclient.o
KEYCLIENT=$(BUILDDIR)/keyclient.o $(BUILDDIR)/keytrans.o
OBJECTS=$(PS2TEST) $(TEXTCON) $(KEYCLIENT) $(MOUSECLIENT)

include $(EROS_SRC)/build/make/makerules.mk
include $(EROS_SRC)/base/tests/drivers/drivers.mk

OPTIM=-O #override!

all: $(TARGETS)

#$(OBJECTS:.o=.c): $(BUILDDIR)/constituents.h
ps2test.c: $(BUILDDIR)/constituents.h
textconsole.c:$(BUILDDIR)/constituents.h
keyclient.c:$(BUILDDIR)/constituents.h
mouseclient.c:$(BUILDDIR)/constituents.h

$(BUILDDIR)/ps2test: $(PS2TEST) $(DOMLIB) $(DOMCRT0)
	$(DOMLINK) $(DOMLINKOPT) -o $@ $(DOMCRT0) $(PS2TEST) $(DOMLIB)

$(BUILDDIR)/textconsole: $(TEXTCON) $(DOMLIB) $(DOMCRT0)
	$(DOMLINK) $(DOMLINKOPT) -o $@ $(DOMCRT0) $(TEXTCON) -lsmall $(DOMLIB)

$(BUILDDIR)/keyclient: $(KEYCLIENT) $(DOMLIB) $(DOMCRT0)
	$(DOMLINK) $(DOMLINKOPT) -o $@  $(DOMCRT0) $(KEYCLIENT) $(DOMLIB)

$(BUILDDIR)/mouseclient: $(MOUSECLIENT) $(DOMLIB) $(DOMCRT0)
	$(DOMLINK) $(DOMLINKOPT) -o $@  $(DOMCRT0) $(MOUSECLIENT) $(DOMLIB)


-include $(BUILDDIR)/.*.m
