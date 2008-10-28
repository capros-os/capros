#
# Copyright (C) 2007, 2008, Strawberry Development Group.
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
# Research Projects Agency under Contract No. W31P4Q-07-C-0070.
# Approved for public release, distribution unlimited.

default: install

HEADERS=linux-emul.h lsync.h

EROS_SRC=../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

VPATH=arch/$(LINUX_TARGET)
INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include -I$(EROS_ROOT)/include/linux-headers
OPTIM=-O
DIRS=
OBJECTS=$(BUILDDIR)/rwsem.o \
  $(BUILDDIR)/alloc.o \
  $(BUILDDIR)/archstubs.o \
  $(BUILDDIR)/completion.o \
  $(BUILDDIR)/cpumask.o \
  $(BUILDDIR)/delay.o \
  $(BUILDDIR)/dma.o \
  $(BUILDDIR)/dmapool.o \
  $(BUILDDIR)/driverCore.o \
  $(BUILDDIR)/ioremap.o \
  $(BUILDDIR)/kmemdup.o \
  $(BUILDDIR)/kobject.o \
  $(BUILDDIR)/kref.o \
  $(BUILDDIR)/kthread.o \
  $(BUILDDIR)/lirq.o \
  $(BUILDDIR)/lthread.o \
  $(BUILDDIR)/lsync.o \
  $(BUILDDIR)/maps.o \
  $(BUILDDIR)/notifier.o \
  $(BUILDDIR)/platform.o \
  $(BUILDDIR)/printk.o \
  $(BUILDDIR)/resource.o \
  $(BUILDDIR)/semaphore.o \
  $(BUILDDIR)/sleep.o \
  $(BUILDDIR)/slob.o \
  $(BUILDDIR)/softirq.o \
  $(BUILDDIR)/string.o \
  $(BUILDDIR)/stubs.o \
  $(BUILDDIR)/threadnum.o \
  $(BUILDDIR)/threadstart.o \
  $(BUILDDIR)/timer.o \
  $(BUILDDIR)/tty_ioctl.o \
  $(BUILDDIR)/utsname.o \
  $(BUILDDIR)/vsprintf.o \
  $(BUILDDIR)/wait.o \
  $(BUILDDIR)/workqueue.o
##  $(BUILDDIR)/page_alloc.o

ifeq "$(EROS_TARGET)" "i386"
OBJECTS+=$(BUILDDIR)/checksum.o
else
ifeq "$(EROS_TARGET)" "arm"
OBJECTS+=$(BUILDDIR)/bitops.o
OBJECTS+=$(BUILDDIR)/csumpartialcopy.o
OBJECTS+=$(BUILDDIR)/devclock.o
OBJECTS+=$(BUILDDIR)/div64.o
OBJECTS+=$(BUILDDIR)/findbit.o
OBJECTS+=$(BUILDDIR)/memzero.o
OBJECTS+=$(BUILDDIR)/traps.o
endif
endif

TARGETS=$(BUILDDIR)/liblinuxk.a $(BUILDDIR)/dstart.o
TARGETS+=$(BUILDDIR)/dyndriverprotospace
TARGETS+=$(BUILDDIR)/dyndriverstart.o

DYNOBJECTS=$(EROS_ROOT)/lib/$(EROS_TARGET)/interpreter.o
DYNOBJECTS+=$(BUILDDIR)/dyndriverprotospace.o

include $(EROS_SRC)/build/make/makerules.mk

$(BUILDDIR)/liblinuxk.a: $(OBJECTS) $(MAKE_BUILDDIR)
	$(RM) $@
	$(AR) -cr $@ $(OBJECTS)
	$(RANLIB) $@

IDL=$(wildcard *.idl)
interfaces: $(IDL)
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros

$(BUILDDIR)/dyndriverprotospace: $(DYNOBJECTS)
	$(EROS_LD) -N -Ttext 0 -static -e _start $(DYNOBJECTS) -o $@

install-headers:
	$(INSTALL) -d $(EROS_ROOT)/include/linuxk
	$(INSTALL) -m 644 $(HEADERS) $(EROS_ROOT)/include/linuxk
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers
	$(INSTALL) -m 644 lprog.map $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 driver.map dyndriver.map $(CAPROS_DOMAIN)/drivers

install: install-headers $(TARGETS)
	$(INSTALL) -m 0644 $(TARGETS) $(EROS_ROOT)/lib/$(EROS_TARGET)

-include $(BUILDDIR)/.*.m
