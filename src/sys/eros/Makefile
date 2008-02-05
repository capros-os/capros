#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

EROS_SRC=../..
include $(EROS_SRC)/build/make/makevars.mk

TARGETS=
DIRS=arch/$(EROS_TARGET) machine
HEADERS=\
	target.h \
	target-asm.h \
	cap-instr.h \
	Invoke.h \
	DevCreKey.h \
	DeviceKey.h \
	KeyboardKey.h \
	ConsoleKey.h \
	SchedCreKey.h \
	Device.h \
	Reserve.h \
	DeviceDef.h \
	KeyConst.h \
	endian.h \
	ffs.h \
	fls.h \
	stdarg.h \
	TimeOfDay.h \
	TimePage.h \
	ProcStats.h \
	CharSrcProto.h \
	TtyKey.h \
	ProcessState.h

IDLSRC=$(wildcard *.idl)

GENERATED=StdKeyType.h
CLEANLIST=$(GENERATED)

include $(EROS_SRC)/build/make/makerules.mk

GENKEYTYPES=../scripts/GenKeyTypes.awk
KEYDEFS=../KEYDEFS

StdKeyType.h: $(KEYDEFS) $(GENKEYTYPES)
	$(EROS_CPP) $(KEYDEFS) | $(GAWK) -f $(GENKEYTYPES) > $@

install: $(GENERATED)
	$(INSTALL) -d $(EROS_ROOT)/include/eros
	$(INSTALL) -m 0644 $(HEADERS) $(GENERATED) $(EROS_ROOT)/include/eros
	$(INSTALL) -d $(EROS_ROOT)/host/include/eros
	$(INSTALL) -m 0644 $(HEADERS) $(GENERATED) $(EROS_ROOT)/host/include/eros

interfaces:
	$(INSTALL) -d $(EROS_ROOT)/idl/capros
	$(INSTALL) -m 0644 $(IDLSRC) $(EROS_ROOT)/idl/capros

.PRECIOUS: $(HEADERS)
