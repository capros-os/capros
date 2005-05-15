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

EROS_SRC=../../..
include $(EROS_SRC)/build/make/makevars.mk

BUILDDIR=.
TARGETS=
DIRS=
#GENERATED=fixregs.h floatregs.h
GENERATED=pseudoregs.h traceregs.h
LOCAL_HEADERS=\
	target.h \
	asm.h \
	atomic.h \
	cap-instr.h \
	endian.h \
	target-asm.h \
	Registers.h \
	io.h \

HEADERS=\
	$(LOCAL_HEADERS) $(GENERATED)

CLEANLIST += fixregs.h floatregs.h pseudoregs.h traceregs.h

include $(EROS_SRC)/build/make/makerules.mk

GENFIXREGS=../../scripts/GenFixRegs.awk
GENFLOATREGS=../../scripts/GenFloatRegs.awk
GENPSEUDOREGS=../../scripts/GenPseudoRegs.awk
GENTRACEREGS=../../scripts/GenTraceRegs.awk
LAYOUT=../../arch/i486/LAYOUT

.FORCE:

fixregs.h: $(LAYOUT) $(GENFIXREGS) .FORCE
	$(EROS_CPP) -I$(EROS_SRC)/sys $(LAYOUT) | $(GAWK) -f $(GENFIXREGS) > $@.tmp
	$(REPLACE) $@.tmp $@

floatregs.h: $(LAYOUT) $(GENFLOATREGS) .FORCE
	$(EROS_CPP) -I$(EROS_SRC)/sys $(LAYOUT) | $(GAWK) -f $(GENFLOATREGS) > $@.tmp
	$(REPLACE) $@.tmp $@

pseudoregs.h: $(LAYOUT) $(GENPSEUDOREGS) .FORCE
	$(EROS_CPP) -I$(EROS_SRC)/sys $(LAYOUT) | $(GAWK) -f $(GENPSEUDOREGS) > $@.tmp
	$(REPLACE) $@.tmp $@

traceregs.h: $(LAYOUT) $(GENTRACEREGS) .FORCE
	$(EROS_CPP) -I$(EROS_SRC)/sys $(LAYOUT) | $(GAWK) -f $(GENTRACEREGS) > $@.tmp
	$(REPLACE) $@.tmp $@


# NOTE that target-asm.h also gets installed in the domain include tree!
install: $(GENERATED)
	$(INSTALL) -d $(EROS_ROOT)/include
	$(INSTALL) -d $(EROS_ROOT)/include/eros
	$(INSTALL) -d $(EROS_ROOT)/include/eros/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/include/eros/$(EROS_TARGET)
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/include
	$(INSTALL) -d $(EROS_ROOT)/host/include/eros
	$(INSTALL) -d $(EROS_ROOT)/host/include/eros/$(EROS_TARGET)
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/host/include/eros/$(EROS_TARGET)

.PRECIOUS: $(LOCAL_HEADERS)
