#
# Copyright (C) 2001, The EROS Group, LLC.
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

# mk -- should be included right after makerules.mk

#CLEANLIST+= sysimg sysvol zsysvol .sysimg.m.* tstflop ztstflop vmfloppy
#CLEANLIST+= mkimage.out

ifndef VOLMAP
VOLMAP=volmap
endif

OPTIM=-O
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include
MAPINC=-I$(EROS_ROOT)/domain -I$(EROS_ROOT)/include
BOOT=$(EROS_ROOT)/lib/$(EROS_TARGET)/image/$(BOOTSTRAP)

# Following is picked up from environment variable if present.
KERNDIR=$(EROS_ROOT)/lib/$(EROS_TARGET)/image
ifeq "" "$(findstring $(EROS_CONFIG).eros.debug,$(wildcard $(KERNDIR)/*))"
KERNEL=$(EROS_CONFIG).eros
else
KERNEL=$(EROS_CONFIG).eros.debug
endif
KERNPATH=$(KERNDIR)/$(KERNEL)
KERNDEP=$(EROS_ROOT)/lib/$(EROS_TARGET)/image/$(KERNEL)

MKIMAGEDEP=$(EROS_ROOT)/src/build/bin/mkimagedep
# so shap can see it better when necessary:
DDBS=1440k
SETVOL_FLAGS += -z

install all: $(TARGETS) $(BUILDDIR)/sysimg

$(BUILDDIR)/sysimg: $(TARGETS) $(IMGMAP)
	$(EROS_ROOT)/host/bin/mkimage -a $(EROS_TARGET) -DBUILDDIR='\"$(BUILDDIR)/\"' -o $(BUILDDIR)/sysimg $(MAPINC) $(IMGMAP) 2>&1 | tee $(BUILDDIR)/mkimage.out

init.hd: $(KERNDEP) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -k $(KERNPATH) $(VOLMAP) $(EROS_HD)

hd: $(BUILDDIR)/sysimg $(KERNDEP) $(VOLMAP)
	$(EROS_ROOT)/host/bin/setvol -D $(EROS_HD)
	cp $(KERNPATH) $(CAPROS_BOOT_PARTITION)/CapROS-kernel-1
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map -g $(CAPROS_BOOT_PARTITION) -v 1 $(EROS_HD) $(BUILDDIR)/sysimg | tee $(BUILDDIR)/sysgen.out

$(BUILDDIR)/sysvol: $(BUILDDIR)/sysimg $(KERNDEP) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAP) $(BUILDDIR)/sysvol
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/sysvol $(BUILDDIR)/sysimg

tstflop: all $(BUILDDIR)/sysvol
	dd if=$(BUILDDIR)/sysvol of=$(EROS_FD) bs=$(DDBS)
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_FD)
	sync
	sleep 5

$(BUILDDIR)/zsysvol: $(BUILDDIR)/sysvol
	cp $(BUILDDIR)/sysvol $(BUILDDIR)/zsysvol
	$(EROS_ROOT)/host/bin/setvol -r $(SETVOL_FLAGS) $(BUILDDIR)/zsysvol

ztstflop: all $(BUILDDIR)/zsysvol
	dd if=$(BUILDDIR)/zsysvol of=$(EROS_FD) bs=$(DDBS)
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_FD)
	sync
	sleep 5

$(BUILDDIR)/vmfloppy: $(BUILDDIR)/zsysvol
	dd if=/dev/zero of=$(BUILDDIR)/vmfloppy bs=1024 count=1440
	dd if=$(BUILDDIR)/zsysvol of=$(BUILDDIR)/vmfloppy conv=notrunc

vmware: $(BUILDDIR) $(BUILDDIR)/vmfloppy
	$(VMWARE) -x -s floppy0.fileType=file -s floppy0.fileName=`pwd`/$(BUILDDIR)/vmfloppy $(HOME)/vmware/EROS/EROS.cfg

bochs: $(BUILDDIR) $(BUILDDIR)/vmfloppy
	bochs -q -f $(EROS_ROOT)/src/build/scripts/bochsrc.eros \
		'boot:a' "floppya: 1_44=$(BUILDDIR)/vmfloppy, status=inserted"
# depend stuff
DEPEND: $(BUILDDIR)/.sysimg.m

# Handles sysimg dependencies.  This line *must* have all of the 
#arguments from the "sysimg:" line, above.

$(BUILDDIR)/.sysimg.m: $(TARGETS) $(IMGMAP)
	-$(MKIMAGEDEP) -DBUILDDIR='"$(BUILDDIR)/"' -o $(BUILDDIR)/sysimg $(MAPINC) $(IMGMAP) $(BUILDDIR)/.sysimg.m >/dev/null  2>&1

-include $(BUILDDIR)/.*.m
