#
# Copyright (C) 2001, The EROS Group, LLC.
# Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

# Set up a kernel and user state for execution. 
 
install: $(BUILDDIR)/sysimg

$(BUILDDIR)/sysimg: $(TARGETS) $(IMGMAP) $(MAKE_BUILDDIR)
	$(MKIMAGE) $(MKIMAGEFLAGS) -o $(BUILDDIR)/sysimg $(IMGMAP) 2>&1
	@$(MKIMAGEDEP) $(MKIMAGEFLAGS) -o $(BUILDDIR)/sysimg $(IMGMAP) $(BUILDDIR)/.sysimg.m >/dev/null  2>&1

$(BUILDDIR)/psysimg: $(TARGETS) $(IMGMAP) $(MAKE_BUILDDIR)
	$(MKIMAGE) $(MKIMAGEFLAGS) -o $(BUILDDIR)/psysimg $(IMGMAP) 2>&1
	@$(MKIMAGEDEP) $(MKIMAGEFLAGS) -o $(BUILDDIR)/psysimg $(IMGMAP) $(BUILDDIR)/.psysimg.m >/dev/null  2>&1

init.hd: $(KERNPATH) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -k $(KERNPATH) $(VOLMAP) $(EROS_HD)

ifndef NPRANGESIZE
NPRANGESIZE=300
endif

ifndef PRANGESIZE
PRANGESIZE=300
endif

// Link kernel and non-persistent objects:
# To run with non-persistent objects only, leave RESTART_CKPT empty
#   and run without a disk.
# To restart from a checkpoint on disk, run with a disk and
#   set RESTART_CKPT to "-p" to properly link device drivers.
np: $(BUILDDIR)/sysimg $(KERNPATH).o
	$(EROS_ROOT)/host/bin/npgen -s $(NPRANGESIZE) $(BUILDDIR)/sysimg $(RESTART_CKPT) $(BUILDDIR)/imgdata
	$(EROS_OBJCOPY) -I binary -O elf32-i386 -B i386 $(BUILDDIR)/imgdata $(BUILDDIR)/imgdata.o
	$(LD) -T $(EROS_SRC)/build/make/sys.$(EROS_TARGET).linkscriptImage -o $(BUILDDIR)/imgdata2.o $(BUILDDIR)/imgdata.o
	$(LD) -static -N -T $(EROS_SRC)/build/make/sys.$(EROS_TARGET).linkscriptKernel -o $(CAPROS_BOOT_PARTITION)/CapROS-kernimg $(KERNPATH).o $(BUILDDIR)/imgdata2.o

// Link kernel and non-persistent objects:
p: $(BUILDDIR)/sysimg $(KERNPATH).o
	$(EROS_ROOT)/host/bin/npgen -s $(NPRANGESIZE) $(BUILDDIR)/sysimg -p $(BUILDDIR)/imgdata
	$(EROS_ROOT)/host/bin/npgen -s $(PRANGESIZE) -b 0x0100000000000000 $(BUILDDIR)/psysimg -a $(BUILDDIR)/imgdata
	$(EROS_OBJCOPY) -I binary -O elf32-i386 -B i386 $(BUILDDIR)/imgdata $(BUILDDIR)/imgdata.o
	$(LD) -T $(EROS_SRC)/build/make/sys.$(EROS_TARGET).linkscriptImage -o $(BUILDDIR)/imgdata2.o $(BUILDDIR)/imgdata.o
	$(LD) -static -N -T $(EROS_SRC)/build/make/sys.$(EROS_TARGET).linkscriptKernel -o $(CAPROS_BOOT_PARTITION)/CapROS-kernimg $(KERNPATH).o $(BUILDDIR)/imgdata2.o

$(BUILDDIR)/sysvol: $(BUILDDIR)/sysimg $(KERNPATH) $(VOLMAP)
	$(EROS_ROOT)/host/bin/mkvol -k $(KERNPATH) $(VOLMAP) $(BUILDDIR)/sysvol
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/sysvol $(BUILDDIR)/sysimg

$(BUILDDIR)/sysvolfd: $(BUILDDIR)/sysimg $(KERNPATH) $(VOLMAPFD)
	$(EROS_ROOT)/host/bin/mkvol -k $(KERNPATH) $(VOLMAPFD) $(BUILDDIR)/sysvolfd
	$(EROS_ROOT)/host/bin/sysgen -m $(BUILDDIR)/sysgen.map $(BUILDDIR)/sysvolfd $(BUILDDIR)/sysimg

tstflop: install $(BUILDDIR)/sysvolfd
	dd if=$(BUILDDIR)/sysvolfd of=$(EROS_FD) bs=1440K
	$(EROS_ROOT)/host/bin/setboot -w $(EROS_FD)
	sync
	sleep 5

$(BUILDDIR)/zsysvol: $(BUILDDIR)/sysvol	# this is broken
	cp $(BUILDDIR)/sysvol $(BUILDDIR)/zsysvol
	$(EROS_ROOT)/host/bin/setvol -r $(SETVOL_FLAGS) $(BUILDDIR)/zsysvol

$(BUILDDIR)/vmfloppy: $(BUILDDIR)/zsysvol
	dd if=/dev/zero of=$(BUILDDIR)/vmfloppy bs=1024 count=1440
	dd if=$(BUILDDIR)/zsysvol of=$(BUILDDIR)/vmfloppy conv=notrunc

vmware: $(BUILDDIR) $(BUILDDIR)/vmfloppy
	$(VMWARE) -x -s floppy0.fileType=file -s floppy0.fileName=`pwd`/$(BUILDDIR)/vmfloppy $(HOME)/vmware/EROS/EROS.cfg

bochs: $(BUILDDIR) $(BUILDDIR)/vmfloppy
	bochs -q -f $(EROS_ROOT)/src/build/scripts/bochsrc.eros \
		'boot:a' "floppya: 1_44=$(BUILDDIR)/vmfloppy, status=inserted"

-include $(BUILDDIR)/.*.m
