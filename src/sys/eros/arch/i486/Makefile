#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, Strawberry Development Group.
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

EROS_SRC=../../../..
EROS_SYS=../../..
include $(EROS_SRC)/build/make/makevars.mk

BUILDDIR=.
TARGETS=
DIRS=
GENERATED=pseudoregs.h traceregs.h
LOCAL_HEADERS=\
	target.h \
	asm.h \
	atomic.h \
	cap-instr.h \
	Invoke.h \
	endian.h \
	target-asm.h \
	Registers.h \
	io.h \

HEADERS=\
	$(LOCAL_HEADERS) $(GENERATED)

IDLSRC=$(wildcard *.idl)

CLEANLIST += pseudoregs.h traceregs.h

include $(EROS_SRC)/build/make/makerules.mk

GENPSEUDOREGS=$(EROS_SYS)/scripts/GenPseudoRegs.awk
GENTRACEREGS=$(EROS_SYS)/scripts/GenTraceRegs.awk
LAYOUT=$(EROS_SYS)/arch/i486/LAYOUT

pseudoregs.h: $(LAYOUT) $(GENPSEUDOREGS)
	$(EROS_CPP) -I$(EROS_SRC)/sys $(LAYOUT) | $(GAWK) -f $(GENPSEUDOREGS) > $@

traceregs.h: $(LAYOUT) $(GENTRACEREGS)
	$(EROS_CPP) -I$(EROS_SRC)/sys $(LAYOUT) | $(GAWK) -f $(GENTRACEREGS) > $@


# NOTE that target-asm.h also gets installed in the domain include tree!
install: $(GENERATED) Makefile
	$(INSTALL) -d $(EROS_ROOT)/include
	$(INSTALL) -d $(EROS_ROOT)/include/eros
	$(INSTALL) -d $(EROS_ROOT)/include/eros/arch/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/include/eros/arch/$(EROS_TARGET)
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/include
	$(INSTALL) -d $(EROS_ROOT)/host/include/eros
	$(INSTALL) -d $(EROS_ROOT)/host/include/eros/arch/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/host/include/eros/arch/$(EROS_TARGET)

interfaces:
	$(INSTALL) -d $(EROS_ROOT)/idl
	$(INSTALL) -d $(EROS_ROOT)/idl/eros
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/arch/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(IDLSRC) $(EROS_ROOT)/idl/capros/arch/$(EROS_TARGET)

.PRECIOUS: $(LOCAL_HEADERS)
