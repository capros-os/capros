#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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


default: install

EROS_SRC=../../..
include $(EROS_SRC)/build/make/makevars.mk


DIRS= $(EROS_TARGET)
# DIRS+= ansiterm call keybits ticktock domtool
# DIRS+= trivkpr call-segkpr nullret parmlack spacebank_test
DIRS+= appstart
DIRS+= extended_node
DIRS+= pcc_test
DIRS+= proccre_test
DIRS+= constructor_test 
# DIRS+= mutex_test
DIRS+= vcs_test
# DIRS+= logtest
# DIRS+= linedisc_test
# DIRS+= crypt_test
# DIRS+= quanta
# DIRS+= lobotomize_test
# DIRS += nullsend reserve_test ipc_times dbvol_test
DIRS+= ipl_test
DIRS+= LocalWindow

CLEANDIRS=${DIRS} vcs_test spacebank_test keyset-test
CLEANDIRS+=timepage fpuswitch old_spacebank_test
CLEANDIRS+=call retag_test rescind_test xtar_test serial_test
CLEANDIRS+=capinstr-test
CLEANDIRS+=retry
CLEANDIRS+=retry-parked
CLEANDIRS+= $(EROS_TARGET)

include $(EROS_SRC)/build/make/makerules.mk
