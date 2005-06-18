#
# Copyright (C) 2001, The EROS Group, LLC.
# Copyright (C) 2005, Strawberry Development Group
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

# test.makeinc -- should be included right after makerules.mk

#CLEANLIST+= test.sysimg test.sysvol ztest.sysvol .test.sysimg.m.* 
#CLEANLIST+= tstflop ztstflop vmfloppy
ifndef VOLMAP
VOLMAP=../bench.volmap
endif

ifndef VOLMAPFD
VOLMAPFD=../bench.volmapfd
endif

IMGMAP=test.imgmap.$(EROS_TARGET)

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

# so shap can see it better when necessary:
DDBS=1440k
SETVOL_FLAGS += -z


install: $(TARGETS) $(BUILDDIR)/test.sysimg

$(BUILDDIR)/test.sysimg: $(TARGETS) $(IMGMAP)
	$(EROS_ROOT)/host/bin/mkimage -a $(EROS_TARGET) -DBUILDDIR='\"$(BUILDDIR)/\"' -o $(BUILDDIR)/test.sysimg $(MAPINC) $(IMGMAP) 2>&1 | tee $(BUILDDIR)/mkimage.out

init.hd: $(KERNDEP) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAP) $(EROS_HD)

test.hd: $(BUILDDIR)/test.sysimg $(KERNDEP) $(VOLMAP)
	$(EROS_ROOT)/host/bin/setvol -b $(BOOT) -k $(KERNPATH) $(EROS_HD)
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_HD)
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(EROS_HD) $(BUILDDIR)/test.sysimg | tee $(BUILDDIR)/sysgen.out

$(BUILDDIR)/test.sysvol: $(BUILDDIR)/test.sysimg $(KERNDEP) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAP) $(BUILDDIR)/test.sysvol
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/test.sysvol $(BUILDDIR)/test.sysimg

$(BUILDDIR)/test.sysvolfd: $(BUILDDIR)/test.sysimg $(KERNDEP) $(VOLMAPFD)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAPFD) $(BUILDDIR)/test.sysvolfd
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/test.sysvolfd $(BUILDDIR)/test.sysimg

tstflop: install $(BUILDDIR)/test.sysvolfd
	dd if=$(BUILDDIR)/test.sysvolfd of=$(EROS_FD) bs=$(DDBS)
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_FD)
	sync
	sleep 5

# zsysvol and ztstflop are for ramdisk load.
$(BUILDDIR)/ztest.sysvol: $(BUILDDIR)/test.sysvol
	cp $(BUILDDIR)/test.sysvol $(BUILDDIR)/ztest.sysvol
	$(EROS_ROOT)/host/bin/setvol -r $(SETVOL_FLAGS) $(BUILDDIR)/ztest.sysvol

ztstflop: install $(BUILDDIR)/ztest.sysvol
	dd if=$(BUILDDIR)/ztest.sysvol of=$(EROS_FD) bs=$(DDBS)
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_FD)
	sync
	sleep 5

$(BUILDDIR)/vmfloppy: $(BUILDDIR)/ztest.sysvol
	dd if=/dev/zero of=$(BUILDDIR)/vmfloppy bs=1024 count=1440
	dd if=$(BUILDDIR)/ztest.sysvol of=$(BUILDDIR)/vmfloppy conv=notrunc

vmware: $(BUILDDIR) $(BUILDDIR)/vmfloppy
	$(VMWARE) -x -s floppy0.fileType=file -s floppy0.fileName=`pwd`/$(BUILDDIR)/vmfloppy $(HOME)/vmware/EROS/EROS.cfg

# depend stuff
DEPEND: $(BUILDDIR)/.test.sysimg.m

# Handles test.sysimg dependencies.  This line *must* have all of the 
#arguments from the "test.sysimg:" line, above.

$(BUILDDIR)/.test.sysimg.m: $(TARGETS) $(IMGMAP)
	-$(MKIMAGEDEP) -DBUILDDIR='"$(BUILDDIR)/"' -o $(BUILDDIR)/test.sysimg $(MAPINC) $(IMGMAP) $(BUILDDIR)/.test.sysimg.m >/dev/null 2>&1

# only include the depend file for this target
-include $(BUILDDIR)/.*.m
