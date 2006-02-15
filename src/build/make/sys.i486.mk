#
# Copyright (C) 2001, The EROS Group, LLC.
# Copyright (C) 2005, 2006, Strawberry Development Group.
#
# This file is part of the CapROS Operating System.
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

# Set up a kernel and domain state for execution. 
 
install: $(BUILDDIR)/sysimg

$(BUILDDIR)/sysimg: $(TARGETS) $(IMGMAP)
	$(EROS_ROOT)/host/bin/mkimage $(MKIMAGEFLAGS) -o $(BUILDDIR)/sysimg $(IMGMAP) 2>&1 | tee $(BUILDDIR)/mkimage.out

init.hd: $(KERNPATH) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAP) $(EROS_HD)

hd: $(BUILDDIR)/sysimg $(KERNPATH)
	$(EROS_ROOT)/host/bin/setvol -D $(EROS_HD)
	cp $(KERNPATH) $(CAPROS_BOOT_PARTITION)/CapROS-kernel-1
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map -g $(CAPROS_BOOT_PARTITION) -v 1 $(EROS_HD) $(BUILDDIR)/sysimg | tee $(BUILDDIR)/sysgen.out

$(BUILDDIR)/sysvol: $(BUILDDIR)/sysimg $(KERNPATH) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAP) $(BUILDDIR)/sysvol
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/sysvol $(BUILDDIR)/sysimg

$(BUILDDIR)/sysvolfd: $(BUILDDIR)/sysimg $(KERNPATH) $(VOLMAPFD)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAPFD) $(BUILDDIR)/sysvolfd
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/sysvolfd $(BUILDDIR)/sysimg

tstflop: install $(BUILDDIR)/sysvolfd
	dd if=$(BUILDDIR)/sysvolfd of=$(EROS_FD) bs=$(DDBS)
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_FD)
	sync
	sleep 5

vmware: $(BUILDDIR) $(BUILDDIR)/vmfloppy
	vmware -x -s floppy0.fileType=file -s floppy0.fileName=`pwd`/$(BUILDDIR)/vmfloppy $(HOME)/vmware/EROS/EROS.cfg

# depend stuff
DEPEND: $(BUILDDIR)/.sysimg.m

# Handles sysimg dependencies.  This line *must* have all of the 
#arguments from the "sysimg:" line, above.

$(BUILDDIR)/.sysimg.m: $(TARGETS) $(IMGMAP)
	-$(MKIMAGEDEP) $(MKIMAGEFLAGS) -o $(BUILDDIR)/sysimg $(IMGMAP) $(BUILDDIR)/.sysimg.m >/dev/null  2>&1

-include $(BUILDDIR)/.*.m
