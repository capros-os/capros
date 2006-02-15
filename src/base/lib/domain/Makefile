#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, Strawberry Development Group.
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

# following is temporary, to help people clean up...
CLEANLIST=$(wildcard */*.[oa]) $(wildcard */*/*.[oa])

EROS_SRC=../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

KERNDIR=$(EROS_SRC)/sys/libkey
CAPSTUBDIR=$(EROS_SRC)/sys/arch/$(EROS_TARGET)/capstubs
VPATH=$(KERNDIR):$(CAPSTUBDIR):domgcc:domdbg:key:crt/$(EROS_TARGET):crt:monitor:
INC=-Iinclude -I$(EROS_ROOT)/include
OPTIM=-O
DIRS=
COMMON_TARGETS=$(BUILDDIR)/libdomain.a $(BUILDDIR)/sbrk.o
ifeq "$(EROS_HOSTENV)" "linux-xenv-gcc3"
MACHINE_OBJECTS=
else
MACHINE_SOURCES=crt0.S crtn.S
MACHINE_OBJECTS=$(patsubst %.S,$(BUILDDIR)/%.o,$(MACHINE_SOURCES))
endif
MACHINE_TARGETS=$(MACHINE_OBJECTS) $(BUILDDIR)/libsmall.a
TARGETS=$(COMMON_TARGETS) $(MACHINE_TARGETS)

KERNSRC=$(notdir $(wildcard $(KERNDIR)/*.c))
CAPSTUBSRC=$(notdir $(wildcard $(CAPSTUBDIR)/*.S))
DBGSRC=$(notdir $(wildcard domdbg/*.c))
KEYSRC=$(notdir $(wildcard key/*.c))
CRTSRC=$(notdir $(wildcard crt/libdom*.c))
MONSRC=$(notdir $(wildcard monitor/*.c))

LIBDOM_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(KERNSRC))
LIBDOM_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(CAPSTUBSRC))
LIBDOM_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(DBGSRC))
LIBDOM_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(KEYSRC))
LIBDOM_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(CRTSRC))
LIBDOM_OBJECTS+=$(patsubst %.c,$(BUILDDIR)/%.o,$(MONSRC))

LIBSMALL_SOURCES=$(wildcard small_*.c) $(notdir $(wildcard small_*.c))
LIBSMALL_SOURCES+=$(notdir $(wildcard crt/small_*.c))
LIBSMALL_SOURCES+=$(notdir $(wildcard crt/$(EROS_TARGET)/small_*.c))
LIBSMALL_HOOK+=small_rt_hook.S
LIBSMALL_OBJECTS=$(patsubst %.c,$(BUILDDIR)/%.o,$(LIBSMALL_SOURCES))
LIBSMALL_OBJECTS+=$(patsubst %.S,$(BUILDDIR)/%.o,$(LIBSMALL_HOOK))

OBJECTS=$(LIBDOM_OBJECTS) $(LIBSMALL_OBJECTS) $(MACHINE_OBJECTS)

HEADERS=$(wildcard include/*/*.h) 

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/libdomain.a: $(LIBDOM_OBJECTS)
	$(AR) -crv $@ $(LIBDOM_OBJECTS)
	$(RANLIB) $@

$(BUILDDIR)/libsmall.a: $(LIBSMALL_OBJECTS)
	$(AR) -crv $@ $(LIBSMALL_OBJECTS)
	$(RANLIB) $@

DIRS=include/domain/drivers
TARDIRS=$(DIRS)

install: all
	$(INSTALL) -d $(EROS_ROOT)/lib
	$(INSTALL) -d $(EROS_ROOT)/lib/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/lib/$(EROS_TARGET)
	$(INSTALL) -d $(EROS_ROOT)/include
	$(INSTALL) -d $(EROS_ROOT)/include/domain
	$(INSTALL) -m 644 $(HEADERS) $(EROS_ROOT)/include/domain
	@for i in $(DIRS); do \
		if [ -d "$$i" ]; then \
			$(MAKE) -C $$i $(MAKERULES) install; \
			if [ $$? -ne 0 ]; then\
				echo "*** JCV: RECURSIVE BUILD STOPS ***";\
				exit 1;\
			fi; \
		fi; \
	done

.PRECIOUS: $(KERNSRC) $(LOCALSRC)

-include .*.m
