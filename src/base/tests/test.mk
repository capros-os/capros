#
# Copyright (C) 2001, The EROS Group, LLC.
# Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group
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

# If VOLMAP is defined, use that,
# otherwise use a local target-specific volmap file if there is one,
# otherwise use the local target-independent volmap file if there is one,
# otherwise use the generic file test.volmap here.
ifndef VOLMAP
VOLMAP=$(if $(wildcard volmap.$(EROS_TARGET)), volmap.$(EROS_TARGET), $(if $(wildcard volmap), volmap, ../../test.volmap))
endif

# Use a target-specific image file if there is one,
# otherwise the target-independent one.
IMGMAP=$(if $(wildcard imgmap.$(EROS_TARGET)), imgmap.$(EROS_TARGET), imgmap)
PIMGMAP=$(if $(wildcard pimgmap.$(EROS_TARGET)), pimgmap.$(EROS_TARGET), pimgmap)

BOOT=$(EROS_ROOT)/lib/$(EROS_TARGET)/image/$(BOOTSTRAP)

KERNDIR=$(EROS_ROOT)/lib/$(EROS_TARGET)/image

# If a kernel option is specified, use it.
# Otherwise, if there is a debug kernel, use it.
# Otherwise, use the nondebug kernel.
# Note, this option is distinct from the NDEBUG option.
ifndef KERNELDEBUG
ifeq "" "$(findstring $(EROS_CONFIG).eros.debug,$(wildcard $(KERNDIR)/*))"
KERNELDEBUG=eros
else
KERNELDEBUG=eros.debug
endif
endif
KERNEL=$(EROS_CONFIG).$(KERNELDEBUG)
KERNPATH=$(KERNDIR)/$(KERNEL)

include $(EROS_SRC)/build/make/sys.$(EROS_TARGET).mk
