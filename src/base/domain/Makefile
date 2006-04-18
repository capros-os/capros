#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, Strawberry Development Group.
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

default: package

EROS_SRC=../..

include $(EROS_SRC)/build/make/makevars.mk

DIRS=
# The order of build among the subdirectories matters. Primordial domains
# really do need to come first.

DIRS+=spacebank
DIRS+=pcc
DIRS+=destroy
DIRS+=constructor
DIRS+=ipltool


# TEMPORARILY REMOVED because not right:
# crypt
#DIRS+=cons_front
DIRS+=console
#DIRS+=dionysix
DIRS+=directory
DIRS+=drivers
DIRS+=echo
#DIRS+=file
DIRS+=hello
DIRS+=idltest
DIRS+=keyset
# DIRS+=linedisc # broken due to RETRY_SET_LIK
DIRS+=mutex
DIRS+=nfile
DIRS+=pipe
DIRS+=sigmux
DIRS+=supernode
DIRS+=vcsk
#DIRS+=displaymgr
# DIRS+=winsys # broken due to RETRY_SET_WAKEINFO
DIRS+=memmap
DIRS+=net 
# DIRS+=eterm # broken due to RETRY_SET_LIK
DIRS+=rangeAl
DIRS+=timer

COREMAP=core_stuff.map
COREMAPWINSYS=core_stuff_winsys.map
CLEANDIRS=$(DIRS) file

include $(EROS_SRC)/build/make/makerules.mk

install:
	$(INSTALL) -d $(EROS_ROOT)/domain
	$(INSTALL) -m 644 $(COREMAP) $(EROS_ROOT)/domain
	$(INSTALL) -m 644 $(COREMAPWINSYS) $(EROS_ROOT)/domain
