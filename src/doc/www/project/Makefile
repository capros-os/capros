#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2008, Strawberry Development Group.
#
# This file is part of the CapROS Operating System,
# and is derived from the EROS Operating System.
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

default: package
EROS_SRC=../../..

include $(EROS_SRC)/build/make/makevars.mk

DOCS= \
	novelty.html \
	schedule.html \
	status.html \
	rationale.html \
	mailing-lists.html \
	release-notes.html \
	build.html \
	names.html \
	team.html \
	UseCases.html

include $(EROS_SRC)/build/make/makerules.mk

CLEANLIST=UseCases.html # generated from UseCases.xml

all: $(DOCS)

install: $(DOCS)
	$(INSTALL) -d $(EROS_ROOT)/doc/www/project
	$(INSTALL) -m 0644 $(DOCS) $(EROS_ROOT)/doc/www/project

-include DEPEND
