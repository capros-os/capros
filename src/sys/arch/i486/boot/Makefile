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

ETAGDIRS=.
COMPRESS=-DSUPPORT_COMPRESSED

OBJECTS=	$(BUILDDIR)/boot1.o 
OBJECTS+=	$(BUILDDIR)/boot2.o 
OBJECTS+=	$(BUILDDIR)/cons_io.o $(BUILDDIR)/bios.o $(BUILDDIR)/asm.o
OBJECTS+=       $(BUILDDIR)/disk_io.o $(BUILDDIR)/bootdisk.o
#
# EVERYTHING above must fit within the low 7680 bytes of
# the bootstrap program, or it will not load!!!
#
OBJECTS += $(BUILDDIR)/main.o $(BUILDDIR)/ramdisk.o $(BUILDDIR)/preload.o 
OBJECTS += $(BUILDDIR)/hibios.o $(BUILDDIR)/loadkern.o $(BUILDDIR)/crc32.o 
OBJECTS += $(BUILDDIR)/memcpy.o $(BUILDDIR)/memset.o $(BUILDDIR)/memory.o
#
# Following is temporary
#
OBJECTS += $(BUILDDIR)/oldmem.o
#
# Following are needed from zlib:
#
ifdef COMPRESS
OBJECTS += $(BUILDDIR)/inflate.o $(BUILDDIR)/infblock.o 
OBJECTS += $(BUILDDIR)/infcodes.o
OBJECTS += $(BUILDDIR)/inftrees.o $(BUILDDIR)/infutil.o
OBJECTS += $(BUILDDIR)/adler32.o $(BUILDDIR)/inffast.o $(BUILDDIR)/malloc.o
endif

CLEANLIST=*.image

DIRS=
TOP=../../..
#INC=-nostdinc -I../include -I$(TOP)
INC=-nostdinc -I../include -I$(EROS_ROOT)/include -I$(TOP)
OPTIM=-O2 -fno-builtin-printf
TARGETS=$(BUILDDIR)/boot
DEF=-DEROS -D__KERNEL__ -DDYNAMIC_CRC_TABLE -DBOOTSTRAP_COMPILE $(COMPRESS)
#DEF=-DDEBUG -DEROS -D__KERNEL__

VPATH=$(TOP)/zlib:.

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

tst: tst.cxx
	$(GPLUS)  $(INC) tst.cxx -o tst

$(BUILDDIR)/boot: $(OBJECTS)
	$(LD) -Ttext 0 -static -o $@ -N $(OBJECTS)

install: all
	$(INSTALL) -d $(EROS_ROOT)/lib/
	$(INSTALL) -d $(EROS_ROOT)/lib/$(EROS_TARGET)
	$(INSTALL) -d $(EROS_ROOT)/lib/$(EROS_TARGET)/image
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/lib/$(EROS_TARGET)/image/

-include $(BUILDDIR)/.*.m
