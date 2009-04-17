#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
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

DIRS=linux-headers
DIRS+=crypto
DIRS+=constructor
DIRS+=idlstub
DIRS+=domain
DIRS+=rbtree util
DIRS+=graphics forwarder addrspace xsprintf
DIRS+= cmme cmte linuxk
DIRS+=ethread
ifneq "$(EROS_TARGET)" "i486"	# not for i486 so far
endif
ifeq "$(EROS_TARGET)" "i486"	# only for i486 so far
endif
# keycache was a failed experiment.
# DIRS += libc unix

include $(EROS_SRC)/build/make/makerules.mk
