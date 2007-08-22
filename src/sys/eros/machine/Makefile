#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, Strawberry Development Group.
#
# This file is part of the CapROS Operating System runtime library.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.

# This material is based upon work supported by the US Defense Advanced
# Research Projects Agency under Contract No. W31P4Q-07-C-0070.
# Approved for public release, distribution unlimited.

default: package

EROS_SRC=../../..
include $(EROS_SRC)/build/make/makevars.mk

BUILDDIR=.
TARGETS=
DIRS=
HEADERS=\
	target.h \
	asm.h \
	atomic.h \
	cap-instr.h \
	Invoke.h \
	endian.h \
	target-asm.h \
	pseudoregs.h \
	traceregs.h \
	io.h \

GENERATED=$(HEADERS)

CLEANLIST += $(GENERATED)

include $(EROS_SRC)/build/make/makerules.mk

GENMACHHEADERS=../../scripts/GenMachHeaders.awk

.FORCE:

$(GENERATED): ../../ARCH-LIST Makefile $(GENMACHHEADERS)
	$(GAWK) -f $(GENMACHHEADERS) $@ < ../../ARCH-LIST


# NOTE that target-asm.h also gets installed in the domain include tree!
install: $(GENERATED)
	$(INSTALL) -d $(EROS_ROOT)/include
	$(INSTALL) -d $(EROS_ROOT)/include/eros
	$(INSTALL) -d $(EROS_ROOT)/include/eros/machine
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/include/eros/machine
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/include
	$(INSTALL) -d $(EROS_ROOT)/host/include/eros
	$(INSTALL) -d $(EROS_ROOT)/host/include/eros/machine
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/host/include/eros/machine

.PRECIOUS: $(LOCAL_HEADERS)
