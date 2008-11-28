#
# Copyright (C) 2001, The EROS Group, LLC.
# Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

# This material is based upon work supported by the US Defense Advanced
# Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
# W31P4Q-07-C-0070.  Approved for public release, distribution unlimited.

# Set up a kernel and user state for execution. 
 
install: $(BUILDDIR)/sysimg

$(BUILDDIR)/sysimg: $(TARGETS) $(IMGMAP) $(MAKE_BUILDDIR)
	$(MKIMAGE) $(MKIMAGEFLAGS) -o $(BUILDDIR)/sysimg $(IMGMAP) 2>&1
	@$(MKIMAGEDEP) $(MKIMAGEFLAGS) -o $(BUILDDIR)/sysimg $(IMGMAP) $(BUILDDIR)/.sysimg.m >/dev/null  2>&1

$(BUILDDIR)/psysimg: $(TARGETS) $(PIMGMAP) $(MAKE_BUILDDIR)
	$(MKIMAGE) $(MKIMAGEFLAGS) -o $(BUILDDIR)/psysimg $(PIMGMAP) 2>&1
	@$(MKIMAGEDEP) $(MKIMAGEFLAGS) -o $(BUILDDIR)/psysimg $(PIMGMAP) $(BUILDDIR)/.psysimg.m >/dev/null  2>&1

init.hd: $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol $(VOLMAP) $(EROS_HD)

BOOTMODULE=$(CAPROS_BOOT_PARTITION)/CapROS-PL-3-1

BOOTDIR=/tftpboot
ARM_SYS=/tftpboot/capros-kernel

ifndef NPRANGESIZE
NPRANGESIZE=310
endif

ifndef PRANGESIZE
PRANGESIZE=310
endif

$(BUILDDIR)/kerneltext $(BUILDDIR)/kerneldata: $(KERNPATH)
	$(EROS_OBJCOPY) -O binary --remove-section=.data --remove-section=.bss $(KERNPATH) $(BUILDDIR)/kerneltext
	$(EROS_OBJCOPY) -O binary --only-section=.data $(KERNPATH) $(BUILDDIR)/kerneldata

// Link kernel and non-persistent objects:
# To run with non-persistent objects only, leave RESTART_CKPT empty
#   and run without a disk.
# To restart from a checkpoint on disk, run with a disk and
#   set RESTART_CKPT to "-p" to properly link device drivers.
np: $(BUILDDIR)/sysimg $(BUILDDIR)/kerneltext $(BUILDDIR)/kerneldata
	$(EROS_ROOT)/host/bin/npgen -s $(NPRANGESIZE) $(BUILDDIR)/sysimg $(RESTART_CKPT) $(BUILDDIR)/imgdata
	cp $(BUILDDIR)/kerneltext $(BUILDDIR)/kerneldata $(BUILDDIR)/imgdata $(BOOTDIR)

// Link kernel, non-persistent objects, and persistent objects:
p: $(BUILDDIR)/sysimg $(BUILDDIR)/psysimg $(BUILDDIR)/kerneltext $(BUILDDIR)/kerneldata
	$(EROS_ROOT)/host/bin/npgen -s $(NPRANGESIZE) $(BUILDDIR)/sysimg -p $(BUILDDIR)/imgdata
	$(EROS_ROOT)/host/bin/npgen -s $(PRANGESIZE) -b 0x0100000000000000 $(BUILDDIR)/psysimg -a $(BUILDDIR)/imgdata
	cp $(BUILDDIR)/kerneltext $(BUILDDIR)/kerneldata $(BUILDDIR)/imgdata $(BOOTDIR)

$(BUILDDIR)/sysvol: $(BUILDDIR)/sysimg $(KERNPATH) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAP) $(BUILDDIR)/sysvol
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/sysvol $(BUILDDIR)/sysimg

$(BUILDDIR)/sysvolfd: $(BUILDDIR)/sysimg $(KERNPATH) $(VOLMAPFD)
	$(EROS_ROOT)/host/bin/mkvol -b $(BOOT) -k $(KERNPATH) $(VOLMAPFD) $(BUILDDIR)/sysvolfd
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/sysvolfd $(BUILDDIR)/sysimg

tstflop: install $(BUILDDIR)/sysvolfd
	dd if=$(BUILDDIR)/sysvolfd of=$(EROS_FD) bs=1440K
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_FD)
	sync
	sleep 5

vmware: $(BUILDDIR) $(BUILDDIR)/vmfloppy
	vmware -x -s floppy0.fileType=file -s floppy0.fileName=`pwd`/$(BUILDDIR)/vmfloppy $(HOME)/vmware/EROS/EROS.cfg

-include $(BUILDDIR)/.*.m
