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

EROS_SRC=../..
include $(EROS_SRC)/build/make/makevars.mk

ifndef VOLMAP
VOLMAP=volmap
endif

TARGETS=$(BUILDDIR)/sysimg
OPTIM=-O
OBJECTS=
CLEANLIST=sysvol sysimg zsysvol sysgen.map vmfloppy
INC=-I$(EROS_ROOT)/include
IPLCRT0=$(EROS_ROOT)/lib/$(EROS_TARGET)/ipldomcrt0.o

KERNDIR=$(EROS_ROOT)/lib/$(EROS_TARGET)/image
ifndef KERNEL
KERNEL=$(EROS_CONFIG).eros
endif
DEBUG_KERNEL=$(findstring $(KERNDIR)/$(KERNEL).debug, $(wildcard $(KERNDIR)/*))
ifneq ($(DEBUG_KERNEL),)
USEKERNEL=$(KERNEL).debug
else
USEKERNEL=$(KERNEL)
endif

KERNPATH=$(KERNDIR)/$(USEKERNEL)


BOOT=$(EROS_ROOT)/lib/$(EROS_TARGET)/image/$(BOOTSTRAP)

include $(EROS_SRC)/build/make/makerules.mk

install all: $(TARGETS)

.force:

$(BUILDDIR)/sysimg: imgmap # .force
	$(EROS_ROOT)/host/bin/mkimage $(MKIMAGEFLAGS) -o $(BUILDDIR)/sysimg imgmap

$(BUILDDIR)/sysvol: $(BUILDDIR)/sysimg  $(KERNPATH) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAP) $(BUILDDIR)/sysvol
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/sysvol $(BUILDDIR)/sysimg

$(BUILDDIR)/zsysvol: $(BUILDDIR)/sysvol
	cp $(BUILDDIR)/sysvol $(BUILDDIR)/zsysvol
	$(EROS_ROOT)/host/bin/setvol -r -z $(BUILDDIR)/zsysvol

tstflop: $(BUILDDIR)/sysvol
	dd if=$(BUILDDIR)/sysvol of=$(EROS_FD)
	sync
	sleep 5

ztstflop: $(BUILDDIR)/zsysvol
	dd if=$(BUILDDIR)/zsysvol of=$(EROS_FD)
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_FD)
	sync
	sleep 5

$(BUILDDIR)/vmfloppy: $(BUILDDIR)/zsysvol
	dd if=/dev/zero of=$(BUILDDIR)/vmfloppy bs=1024 count=1440
	dd if=$(BUILDDIR)/zsysvol of=$(BUILDDIR)/vmfloppy conv=notrunc

vmware: $(BUILDDIR)/vmfloppy
	$(VMWARE) -x -s floppy0.fileType=file -s floppy0.fileName=`pwd`/$(BUILDDIR)/vmfloppy $(HOME)/vmware/EROS/EROS.cfg

$(BUILDDIR)/.sysimg.m: $(TARGETS) $(IMGMAP)
	@-$(MKIMAGEDEP) $(MKIMAGEFLAGS) -o test.sysimg $(IMGMAP) .sysimg.m >/dev/null  2>&1

# only include the depend file for this target
-include $(BUILDDIR)/.*.m
