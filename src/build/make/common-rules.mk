#
# Copyright (C) 2003, Jonathan S. Shapiro.
# Copyright (C) 2005, 2009, 2010, Strawberry Development Group
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

unexport DIRS
unexport ETAGDIRS
unexport GENERATED
unexport CLEANLIST

ifndef MAKEVARS_LOADED
include $(EROS_SRC)/build/make/makevars.mk
endif

ifeq "$(BUILDDIR)" ""
$(error BUILDDIR is not set!)
endif
ifneq "$(BUILDDIR)" "."
CLEAN_BUILDDIR=$(BUILDDIR)
endif

.PHONY: tags
.PHONY: world package

######################################################################
#
# TOP LEVEL BUILD RULES FOR REMAKING CURRENT PACKAGE OR ENTIRE WORLD
#
######################################################################
world:
	$(MAKE) -C $(EROS_SRC) $(MAKERULES) packages

ifneq "$(PACKAGE)" ""
package:
	$(MAKE) -C $(EROS_ROOT)/src/$(PACKAGE) $(MAKERULES) interfaces
	$(MAKE) -C $(EROS_ROOT)/src/$(PACKAGE) $(MAKERULES) libs
	$(MAKE) -C $(EROS_ROOT)/src/$(PACKAGE) $(MAKERULES) install

endif

######################################################################
#
# GLOBAL AND RECURSIVE TARGETS FOR USE WITHIN A PACKAGE.
#
######################################################################

# IDIRS is a list of directories to recurse on for "interfaces".
# LDIRS is a list of directories to recurse on for "libs".
# PDIRS is a list of directories to recurse on for "install".
# DIRS is a list of additional directories to recurse on
#   for "interfaces" and "install" (not "libs").

.PHONY: subdirs
subdirs:
	@for i in $(RECURSE_DIRS); do \
		if [ -d "$$i" ]; then\
			$(MAKE) -C $$i $(MAKERULES) $(RECURSE_TARGET); \
			if [ $$? -ne 0 ]; then\
				echo "*** RECURSIVE BUILD STOPS ***";\
				exit 1;\
			fi; \
		fi; \
	done

.PHONY: install
install: RECURSE_TARGET=install
# DIRS and PDIRS should not have duplicate entries.
install: RECURSE_DIRS=$(DIRS) $(PDIRS)
install: subdirs

.PHONY: interfaces
interfaces: RECURSE_TARGET=interfaces
# DIRS and IDIRS should not have duplicate entries.
interfaces: RECURSE_DIRS=$(DIRS) $(IDIRS)
interfaces: subdirs

.PHONY: libs
libs: RECURSE_TARGET=libs
libs: RECURSE_DIRS=$(LDIRS)	# recurse over LDIRS only
libs: subdirs

# Target clean removes generated files in the current directory
# and CLEANDIRS recursively. 
# It also removes files in CLEANLIST.
# Local Makefiles can add dependencies to nonrecursiveClean. 

ifndef CLEANDIRS
CLEANDIRS=$(DIRS) $(IDIRS) $(LDIRS) $(PDIRS)
endif

.PHONY: clean

clean: nodepend
clean: RECURSE_TARGET=clean
# Remove duplicates from CLEANDIRS
clean: RECURSE_DIRS=$(shell echo $(CLEANDIRS) | tr " " "\n" | sort | uniq)
clean: nonrecursiveClean subdirs
nonrecursiveClean: generic-clean

.PHONY: generic-clean
generic-clean:
	-rm -f *.o core *~ new.Makefile  ".#"*
	-rm -f sysgen.map TAGS
	-rm -f *.dvi *.blg *.aux *.log *.toc $(CLEANLIST)
ifneq "$(CLEAN_BUILDDIR)" ""
	-rm -rf $(CLEAN_BUILDDIR)
endif

## CLEANING: The following piece of idiocy works around the fact that
## the autodependency files may refer to stuff that has been cleaned,
## and that this can therefore perversely cause the clean target to
## fail.

.PHONY: nodepend 
nodepend:
	-find . -name '.*.m' -exec rm {} \;
	-find . -name 'BUILD' | xargs -n 40 rm -rf \;

.PHONY: webify
ifdef ETAGDIRS
webify: tags
endif
webify: $(BUILDDIR) $(GENERATED)
webify: RECURSE_TARGET=webify
webify: RECURSE_DIRS=$(DIRS)
webify: subdirs

# This is a debugging target..
.PHONY: walk
walk: RECURSE_TARGET=walk
walk: RECURSE_DIRS=$(DIRS)
walk: subdirs

here:
	@echo $(PWD)

COMMONRULES_LOADED=1
