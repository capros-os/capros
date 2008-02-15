#
# Copyright (C) 2003, Jonathan S. Shapiro.
# Copyright (C) 2005, Strawberry Development Group
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


unexport DIRS
unexport ETAGDIRS
unexport GENERATED
unexport CLEANLIST

ifndef MAKEVARS_LOADED
include $(EROS_SRC)/build/make/makevars.mk
endif

ifndef CLEANDIRS
CLEANDIRS=$(DIRS)
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

export RECURSE_TARGET

.PHONY: subdirs
subdirs:
	@for i in $(DIRS); do \
		if [ -d "$$i" ]; then\
			$(MAKE) -C $$i $(MAKERULES) $(RECURSE_TARGET); \
			if [ $$? -ne 0 ]; then\
				echo "*** RECURSIVE BUILD STOPS ***";\
				exit 1;\
			fi; \
		fi; \
	done

.PHONY: recurse
recurse:
	@for i in $(DIRS); do \
		if [ -d "$$i" ]; then\
			$(MAKE) -C $$i $(MAKERULES) $(RECURSE_TARGET) ; \
			if [ $$? -ne 0 ]; then\
				echo "*** RECURSIVE BUILD STOPS ***";\
				exit 1;\
			fi; \
		fi; \
	done

.PHONY: recurseClean
recurseClean:
	@for i in $(CLEANDIRS); do \
		if [ -d "$$i" ]; then\
			$(MAKE) -C $$i $(MAKERULES) recurseClean $(RECURSE_TARGET) ; \
			if [ $$? -ne 0 ]; then\
				echo "*** RECURSIVE BUILD STOPS ***";\
				exit 1;\
			fi; \
		fi; \
	done

.PHONY: install
install: RECURSE_TARGET=install
install: recurse

.PHONY: interfaces
interfaces: RECURSE_TARGET=interfaces
interfaces: recurse

### install: recursive-install
### recursive-install:
### ifneq "$(DIRS)" ""
### 	@for i in $(DIRS); do \
### 		if [ -d "$$i" ]; then\
### 			$(MAKE) -C $$i $(MAKERULES) install; \
### 			if [ $$? -ne 0 ]; then\
### 				echo "*** RECURSIVE BUILD STOPS ***";\
### 				exit 1;\
### 			fi; \
### 		fi; \
### 	done
### endif

# Target clean removes generated files in the current directory
# and CLEANDIRS recursively. 
# It also removes files in CLEANLIST.
# Local Makefiles can add dependencies to nonrecursiveClean. 

.PHONY: clean

clean: nodepend
clean: RECURSE_TARGET=nonrecursiveClean
clean: nonrecursiveClean recurseClean 
nonrecursiveClean: generic-clean

.PHONY: generic-clean
generic-clean:
	-rm -f *.o core *~ new.Makefile  ".#"*
	-rm -f .*.m sysgen.map $(TARGETS) TAGS
	-rm -f *.dvi *.blg *.aux *.log *.toc $(CLEANLIST)
	-rm -rf idl
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
webify: recurse

# This is a debugging target..
.PHONY: walk
walk: RECURSE_TARGET=walk
walk: recurse

here:
	@echo $(PWD)

COMMONRULES_LOADED=1
