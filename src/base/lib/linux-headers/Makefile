#
# Copyright (C) 2007, Strawberry Development Group.
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

LINUX_HEADERS=$(wildcard linux/*.h)
LINUXAMBA_HEADERS=$(wildcard linux/amba/*.h)
LINUXBYTEORDER_HEADERS=$(wildcard linux/byteorder/*.h)
LINUXSUNRPC_HEADERS=$(wildcard linux/sunrpc/*.h)
LINUXUSB_HEADERS=$(wildcard linux/usb/*.h)
ASMGENERIC_HEADERS=$(wildcard asm-generic/*.h)
ASMGENERICBITOPS_HEADERS=$(wildcard asm-generic/bitops/*.h)
NET_HEADERS=$(wildcard net/*.h)
SCSI_HEADERS=$(wildcard scsi/*.h)

EROS_SRC=../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

DIRS=asm

# FIXME: Should be using ARCH_LIST to get the Linux name:
ifeq "$(EROS_TARGET)" "i486"
DIRS+=asm-i386
else
DIRS+=asm-$(EROS_TARGET)
endif

include $(EROS_SRC)/build/make/makerules.mk

install:
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/linux/
	$(INSTALL) -m 644 $(LINUX_HEADERS) $(EROS_ROOT)/include/linux-headers/linux/
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/linux/amba/
	$(INSTALL) -m 644 $(LINUXAMBA_HEADERS) $(EROS_ROOT)/include/linux-headers/linux/amba/
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/linux/byteorder/
	$(INSTALL) -m 644 $(LINUXBYTEORDER_HEADERS) $(EROS_ROOT)/include/linux-headers/linux/byteorder/
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/linux/sunrpc/
	$(INSTALL) -m 644 $(LINUXSUNRPC_HEADERS) $(EROS_ROOT)/include/linux-headers/linux/sunrpc/
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/linux/usb/
	$(INSTALL) -m 644 $(LINUXUSB_HEADERS) $(EROS_ROOT)/include/linux-headers/linux/usb/
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/asm-generic/
	$(INSTALL) -m 644 $(ASMGENERIC_HEADERS) $(EROS_ROOT)/include/linux-headers/asm-generic/
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/asm-generic/bitops/
	$(INSTALL) -m 644 $(ASMGENERICBITOPS_HEADERS) $(EROS_ROOT)/include/linux-headers/asm-generic/bitops/
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/net/
	$(INSTALL) -m 644 $(NET_HEADERS) $(EROS_ROOT)/include/linux-headers/net/
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/scsi/
	$(INSTALL) -m 644 $(SCSI_HEADERS) $(EROS_ROOT)/include/linux-headers/scsi/
