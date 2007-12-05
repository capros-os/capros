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

default: all

ASMARCH=default
ASMARCH_DIR=mach-$(ASMARCH)

ASM_HEADERS=$(wildcard *.h)
ASMARCH_HEADERS=$(wildcard $(ASMARCH_DIR)/*.h)

THISDIR=$(notdir $(shell pwd))

EROS_SRC=../../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

DIRS=

include $(EROS_SRC)/build/make/makerules.mk

all:

install: all
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/$(THISDIR)/$(ASMARCH_DIR)/
	$(INSTALL) -m 644 $(ASM_HEADERS) $(EROS_ROOT)/include/linux-headers/$(THISDIR)/
	$(INSTALL) -m 644 $(ASMARCH_HEADERS) $(EROS_ROOT)/include/linux-headers/$(THISDIR)/$(ASMARCH_DIR)/
