#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006-2010, Strawberry Development Group.
# Copyright (C) 2021, William ML Leslie
#
# This file is part of the CapROS Operating System runtime library,
# and is derived from the EROS Operating System runtime library.
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

HEADERS=\
	cap-instr.h \
	CkptRoot.h \
	DiskGPT.h \
	DiskLSS.h \
	DiskNode.h \
	DiskObjDescr.h \
	DivTypes.h \
	ErosTypes.h \
	Forwarder.h \
	GenerationHdr.h \
	Key-inline.h \
	KeyStruct.h \
	LowVolume.h \
	NPODescr.h \
	Node.h \
	TagPot.h

include $(EROS_SRC)/build/make/makerules.mk

interfaces: $(GENERATED)
	$(INSTALL) -d $(EROS_ROOT)/include/disk
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/include/disk
	$(INSTALL) -d $(EROS_ROOT)/host/include/disk
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/host/include/disk


