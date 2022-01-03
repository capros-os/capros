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

default: package
EROS_SRC=../../../../..
include $(EROS_SRC)/build/make/makevars.mk

DOCS= \
	00Concepts.html \
	SensoryKey.html \
	Constructor.html \
	Prompt.html \
	Keeper.html \
	Scheduling.html \
	WorkingSets.html \
	AddressSpaces.html \
	Invocation.html

include $(EROS_SRC)/build/make/makerules.mk

all:

install:
	$(INSTALL) -d $(EROS_ROOT)/doc
	$(INSTALL) -d $(EROS_ROOT)/doc/www
	$(INSTALL) -d $(EROS_ROOT)/doc/www/devel
	$(INSTALL) -d $(EROS_ROOT)/doc/www/devel/ObRef
	$(INSTALL) -d $(EROS_ROOT)/doc/www/devel/ObRef/concepts
	$(INSTALL) -m 0644 $(DOCS) $(EROS_ROOT)/doc/www/devel/ObRef/concepts

-include DEPEND
