#
# Copyright (C) 2001, The EROS Group, LLC.
# Copyright (C) 2005, 2006, Strawberry Development Group
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

# test.makeinc -- should be included right after makerules.mk

#CLEANLIST+= sysimg sysvol .sysimg.m.* 
#CLEANLIST+= tstflop ztstflop vmfloppy
ifndef VOLMAP
VOLMAP=../bench.volmap
endif

IMGMAP=imgmap.$(EROS_TARGET)

BOOT=$(EROS_ROOT)/lib/$(EROS_TARGET)/image/$(BOOTSTRAP)

# Following is picked up from environment variable if present.
KERNDIR=$(EROS_ROOT)/lib/$(EROS_TARGET)/image
ifeq "" "$(findstring $(EROS_CONFIG).eros.debug,$(wildcard $(KERNDIR)/*))"
KERNEL=$(EROS_CONFIG).eros
else
KERNEL=$(EROS_CONFIG).eros.debug
endif
KERNPATH=$(KERNDIR)/$(KERNEL)

# so shap can see it better when necessary:
DDBS=1440k
SETVOL_FLAGS += -z

include $(EROS_SRC)/build/make/sys.$(EROS_TARGET).mk
