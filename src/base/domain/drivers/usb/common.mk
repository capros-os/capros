#
# Copyright (C) 2010, Strawberry Development Group.
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

VPATH+=$(EROS_SRC)/base/domain/drivers/usb/core:$(EROS_SRC)/base/domain/drivers/usb/host

EHCI_OBJECTS=
EHCI_OBJECTS+= $(BUILDDIR)/ehci-hcd.o
EHCI_OBJECTS+= $(BUILDDIR)/ehci_init.o
EHCI_OBJECTS+= $(CAPROS_DOMAIN)/libpcidev.a

UHCI_OBJECTS=
UHCI_OBJECTS+= $(BUILDDIR)/uhci-hcd.o
UHCI_OBJECTS+= $(BUILDDIR)/pci-quirks.o

COMMON_OBJECTS=
COMMON_OBJECTS+=$(BUILDDIR)/buffer.o
COMMON_OBJECTS+=$(BUILDDIR)/config.o
COMMON_OBJECTS+=$(BUILDDIR)/driver.o
COMMON_OBJECTS+=$(BUILDDIR)/generic.o
COMMON_OBJECTS+=$(BUILDDIR)/hcd.o
COMMON_OBJECTS+=$(BUILDDIR)/hub.o
COMMON_OBJECTS+=$(BUILDDIR)/message.o
COMMON_OBJECTS+=$(BUILDDIR)/quirks.o
COMMON_OBJECTS+=$(BUILDDIR)/usb.o
COMMON_OBJECTS+=$(BUILDDIR)/urb.o
COMMON_OBJECTS+=$(BUILDDIR)/cap.o

INC=$(DRIVERINC)
