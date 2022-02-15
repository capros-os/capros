#
# Copyright (C) 2001, 2001, Jonathan S. Shapiro.
# Copyright (C) 2007, 2010, Strawberry Development Group.
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

default: interfaces

EROS_SRC=../../..

BUILDDIR=BUILD/$(EROS_TARGET)

include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/capidl
CLEANLIST= ../../bin/capidl
OPTIM=-g

LIBAPP_OBJECTS = $(BUILDDIR)/Intern.o
LIBAPP_OBJECTS += $(BUILDDIR)/App.o
LIBAPP_OBJECTS += $(BUILDDIR)/Diag.o
LIBAPP_OBJECTS += $(BUILDDIR)/path.o
LIBAPP_OBJECTS += $(BUILDDIR)/sha1.o
LIBAPP_OBJECTS += $(BUILDDIR)/buffer.o
LIBAPP_OBJECTS += $(BUILDDIR)/xmalloc.o
LIBAPP_OBJECTS += $(BUILDDIR)/PtrVec.o

CAPIDL_OBJECTS= $(BUILDDIR)/idl.o
CAPIDL_OBJECTS += $(BUILDDIR)/lex.o
CAPIDL_OBJECTS += $(BUILDDIR)/SymTab.o
CAPIDL_OBJECTS += $(BUILDDIR)/prescan.o
CAPIDL_OBJECTS += $(BUILDDIR)/capidl.o
CAPIDL_OBJECTS += $(BUILDDIR)/util.o
CAPIDL_OBJECTS += $(BUILDDIR)/output.o
CAPIDL_OBJECTS += $(BUILDDIR)/rewrite_c.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_symdump.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_xmldoc.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_c_hdr.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_c_hdr_depend.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_c_stubs.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_c_server.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_c_server_hdr.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_c_stub_depend.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_capidl.o
CAPIDL_OBJECTS += $(BUILDDIR)/o_depend.o

OBJECTS=$(LIBAPP_OBJECTS) $(CAPIDL_OBJECTS)

INC=	-I. -I$(BUILDDIR)
LIBS=$(BUILDDIR)/libapp.a
OTHER_LIBS= -liberty -lgmp -lstdc++
DIRS=
VPATH=.:applib

include $(EROS_SRC)/build/make/makerules.mk

$(BUILDDIR)/libapp.a: $(LIBAPP_OBJECTS)
	ar -crv $(BUILDDIR)/libapp.a $(LIBAPP_OBJECTS)

#$(BUILDDIR)/capidl.a: $(CAPIDL_OBJECTS) $(LIBS)
#	ar -crv $(BUILDDIR)/capidl.a $(CAPIDL_OBJECTS)

$(BUILDDIR)/capidl: $(CAPIDL_OBJECTS) $(LIBS)
	$(GPLUS) $(GPLUSFLAGS) -o $@ $(CAPIDL_OBJECTS) $(XENV_LIBDIR) $(LIBS) $(OTHER_LIBS)

$(BUILDDIR)/idl.o: $(BUILDDIR)/idl.c
	$(C_BUILD) 
	$(C_DEP)

$(BUILDDIR)/lex.o: lex.c $(BUILDDIR)/idl.h
# Can't use $(C_BUILD) because it has -Werror and lex.c generates a warning.
	$(GCC) $(GCCFLAGS) $(MOPSWARN) -Werror -Wno-unused-function -c $< -o $@ 
	$(C_DEP)

$(BUILDDIR)/prescan.o: prescan.c $(BUILDDIR)/idl.h
# Can't use $(C_BUILD) because it has -Werror and prescan.c generates a warning.
	$(GCC) $(GCCFLAGS) $(MOPSWARN) -Werror -Wno-unused-function -c $< -o $@ 
	$(C_DEP)

.NOTPARALLEL:
# the bison rule below is not re-entrant.


$(BUILDDIR)/idl.c $(BUILDDIR)/idl.h: idl.y $(MAKE_BUILDDIR)
	(cd $(BUILDDIR);bison -v -d -y ../../idl.y)
	(cd $(BUILDDIR); mv y.tab.c idl.c)
	(cd $(BUILDDIR); mv y.tab.h idl.h)

$(BUILDDIR)/prescan.c: prescan.l $(MAKE_BUILDDIR)
	flex -d -o$@ prescan.l

$(BUILDDIR)/lex.c: lex.l
	flex -d -o$@ lex.l

nonrecursiveClean:
	rm lex.c prescan.c 

interfaces: $(TARGETS)
	$(INSTALL) -m 755 $(TARGETS) ../../bin

-include $(BUILDDIR)/.*.m
