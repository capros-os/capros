#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2008, 2009, Strawberry Development Group.
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

EROS_SRC=../..

include $(EROS_SRC)/build/make/makevars.mk

DIRS=
# The order of build among the subdirectories matters.
DIRS+=openssl	# openssl should probably be in base/lib instead of here.

DIRS+=spacebank
DIRS+=pcc
DIRS+=constructor
DIRS+=ipltool


# TEMPORARILY REMOVED because not right:
# crypt
#DIRS+=cons_front
DIRS+=ckptr
DIRS+=console
DIRS+=w1mult
DIRS+=nplink
#DIRS+=dionysix
DIRS+=directory
DIRS+=drivers
DIRS+=echo
#DIRS+=file
DIRS+=hai
DIRS+=hello
DIRS+=idltest
DIRS+=ip
DIRS+=keyset
# DIRS+=linedisc # broken due to RETRY_SET_LIK
DIRS+=logfile
DIRS+=loghttp
DIRS+=mutex
DIRS+=nfile
DIRS+=pipe
DIRS+=sigmux
DIRS+=supernode
DIRS+=swca
DIRS+=vcsk
#DIRS+=displaymgr
# DIRS+=winsys # broken due to RETRY_SET_WAKEINFO
DIRS+=memmap
# DIRS+=net # from EROS, not used in CapROS
# DIRS+=eterm # broken due to RETRY_SET_LIK
# DIRS+=rangeAl # broken, compilation errors
DIRS+=timer

DIRS+=devclock
DIRS+=netlistener
DIRS+=http

COREMAP=core_stuff.map
COREMAPWINSYS=core_stuff_winsys.map
CLEANDIRS=$(DIRS) file

include $(EROS_SRC)/build/make/makerules.mk

install:
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 $(COREMAP) $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 $(COREMAPWINSYS) $(CAPROS_DOMAIN)
