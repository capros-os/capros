#
# Copyright (C) 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, Strawberry Development Group
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
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include
include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=console.map
TARGETS =$(BUILDDIR)/consolestream $(BUILDDIR)/textconsole\
	$(BUILDDIR)/keyclient $(BUILDDIR)/mouseclient
CONSOLESTREAM=$(BUILDDIR)/consolestream.o
TEXTCON=$(BUILDDIR)/textconsole.o
KEYCLIENT=$(BUILDDIR)/keyclient.o $(BUILDDIR)/keytrans.o
MOUSECLIENT=$(BUILDDIR)/mouseclient.o
OBJECTS=$(CONSOLESTREAM) $(TEXTCON) $(KEYCLIENT) $(MOUSECLIENT)
IDL=textconsole.idl keyclient.idl
LIBSTUB=idl.a
CLEANLIST=idl.a
LIBS=-lethread

include $(EROS_SRC)/build/make/makerules.mk

INC+=-I.
OPTIM=-O #override!

all: idl
	$(MAKE) $(TARGETS)

consolestream.c: $(BUILDDIR)/constituents.h
textconsole.c:$(BUILDDIR)/constituents.h
keyclient.c:$(BUILDDIR)/constituents.h
mouseclient.c:$(BUILDDIR)/constituents.h

stub-depend: $(IDL) $(MAKE_BUILDDIR)
	$(CAPIDL) -D $(BUILDDIR) -l c-stubs -I $(EROS_ROOT)/idl $(IDL)
	$(CAPIDL) -D $(BUILDDIR) -X $(BUILDDIR) -Aidl.a -l c-stub-depend -I $(EROS_ROOT)/idl $(IDL) > $(BUILDDIR)/.idl-stubs.m

idl-headers: $(IDL)
	$(CAPIDL) -D idl/ -l c-header -I $(EROS_ROOT)/idl $(IDL)

$(BUILDDIR)/%.o: $(BUILDDIR)/%.c
	$(C_BUILD)

idl: idl-headers stub-depend
	$(MAKE) $(BUILDDIR)/idl.a

$(BUILDDIR)/consolestream: $(CONSOLESTREAM) $(DOMLIB) $(DOMCRT0) $(DOMSBRK)
# Temporarily including sbrk.o explicitly.
# It should be in libc. It is required by malloc.
	$(CROSSLINK) -o $@ $(CONSOLESTREAM) $(DOMSBRK) $(LIBS) $(CROSSLIBS) $(BUILDDIR)/idl.a

$(BUILDDIR)/textconsole: $(TEXTCON) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(TEXTCON) $(SMALL_SPACE) $(CROSSLIBS)

$(BUILDDIR)/keyclient: $(KEYCLIENT) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(KEYCLIENT) $(CROSSLIBS) $(BUILDDIR)/idl.a

$(BUILDDIR)/mouseclient: $(MOUSECLIENT) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(MOUSECLIENT) $(CROSSLIBS)

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers/console
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers/console
	$(INSTALL) -m 644 console.map $(CAPROS_DOMAIN)/drivers/console
-include $(BUILDDIR)/.*.m
