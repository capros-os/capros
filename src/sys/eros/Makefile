#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
#
# This file is part of the EROS Operating System runtime library.
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
#
default: package

EROS_SRC=../..
include $(EROS_SRC)/build/make/makevars.mk

TARGETS=
DIRS=i486 machine
HEADERS=\
	target.h \
	target-asm.h \
	cap-instr.h \
	Invoke.h \
	NodeKey.h \
	PageKey.h \
	DevCreKey.h \
	DeviceKey.h \
	KeyboardKey.h \
	ConsoleKey.h \
	SchedCreKey.h \
	Device.h \
	Reserve.h \
	DeviceDef.h \
	KeyConst.h \
	SegmentKey.h \
	SegKeeperInfo.h \
	endian.h \
	stdarg.h \
	TimeOfDay.h \
	TimePage.h \
	Registers.h \
	ProcStats.h \
	CharSrcProto.h \
	TtyKey.h \
	ProcessKey.h \
	ProcessState.h

IDLSRC=$(wildcard *.idl)

GENERATED=StdKeyType.h
CLEANLIST=$(GENERATED)

include $(EROS_SRC)/build/make/makerules.mk

GENKEYTYPES=../scripts/GenKeyTypes.awk
KEYDEFS=../KEYDEFS

.FORCE:

StdKeyType.h: $(KEYDEFS)  .FORCE
	$(EROS_CPP) $(KEYDEFS) | $(GAWK) -f $(GENKEYTYPES) > $@.tmp
	$(REPLACE) $@.tmp $@

install: $(GENERATED)
	$(INSTALL) -d $(EROS_ROOT)/include
	$(INSTALL) -d $(EROS_ROOT)/include/eros
	$(INSTALL) -m 0644 $(HEADERS) $(GENERATED) $(EROS_ROOT)/include/eros
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/include
	$(INSTALL) -d $(EROS_ROOT)/host/include/eros
	$(INSTALL) -m 0644 $(HEADERS) $(GENERATED) $(EROS_ROOT)/host/include/eros

interfaces:
	$(INSTALL) -d $(EROS_ROOT)/idl
	$(INSTALL) -d $(EROS_ROOT)/idl/eros
	$(INSTALL) -m 0644 $(IDLSRC) $(EROS_ROOT)/idl/eros

.PRECIOUS: $(HEADERS)
