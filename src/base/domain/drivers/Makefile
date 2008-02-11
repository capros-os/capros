#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2008, Strawberry Development Group.
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

default: package

EROS_SRC=../../..
include $(EROS_SRC)/build/make/makevars.mk

DIRS=
DIRS+=serial
ifeq "$(EROS_TARGET)" "i486"	# drivers are for i486 only so far
# DIRS+=pci # disabled due to compilation errors
# DIRS+=ps2 # broken due to RETRY_SET_WAKEINFO
DIRS+=sound
DIRS+=network
# DIRS+=console # broken due to RETRY_SET_LIK
else	# ARM-only drivers
##DIRS+=usb	# so far, ARM only
endif

include $(EROS_SRC)/build/make/makerules.mk

