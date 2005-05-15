#
# Copyright (C) 2001, The EROS Group, LLC.
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


default: world


# DIRS are ordered:
# build must be made before others. base and doc can be in either order.
# Nothing depends on legal.
DIRS=legal sys base doc tutorial
DIRS+=uncompleted dionysix
DIRS+=tcp-ip muir-ip muir-tcp
CLEANDIRS=build $(DIRS)

STALE_DIRS=xenv examples

UPDATE_DIRS=$(DIRS) xenv examples

TARDIRS=$(DIRS)

EROS_SRC=.
include $(EROS_SRC)/build/make/pkgrules.mk

distclean: targdir-clean clean

targdir-clean:
	-rm -rf $(EROS_ROOT)/include $(EROS_ROOT)/lib $(EROS_ROOT)/idl
	-rm -rf $(EROS_ROOT)/domain $(EROS_ROOT)/doc
	-rm -rf $(EROS_ROOT)/cross $(EROS_ROOT)/host

ifeq "$(NEED_CROSS_BOOTSTRAP)" "yes"
cross-bootstrap:
	$(MAKE) -C base/ccs PACKAGE=base $(MAKERULES) cross-bootstrap
endif

packages:
	$(MAKE) interfaces
	$(MAKE) libs
	$(MAKE) install

build-pkg:
	$(MAKE) -C build PACKAGE=build $(MAKERULES) package

# The following do not use the generic recurse target because we do not
# want them to recurse deeply -- only to one level. Also, we need to 
# re-set the PACKAGE variable for each of them.
interfaces: build-pkg
interfaces: RECURSE_TARGET=interfaces
interfaces: subdirs
interfaces: idl-headers

idl-headers:
	$(CAPIDL) -D $(EROS_ROOT)/include/idl.tmp -l c-header `find $(EROS_ROOT)/idl -name '*.idl'`
	$(INSTALL) -d $(EROS_ROOT)/include
	$(INSTALL) -d $(EROS_ROOT)/include/idl
	build/bin/dirsync $(EROS_ROOT)/include/idl.tmp $(EROS_ROOT)/include/idl
	$(INSTALL) -d $(EROS_ROOT)/host/include
	$(INSTALL) -d $(EROS_ROOT)/host/include/idl
	build/bin/dirsync $(EROS_ROOT)/include/idl.tmp $(EROS_ROOT)/host/include/idl
	-rm -rf $(EROS_ROOT)/include/idl.tmp

libs: build-pkg
libs: RECURSE_TARGET=libs
libs: subdirs

# The install target recurses deeply in any case.
install: build-pkg

tar:	clean
	-rm -f eros.tar eros.tar.gz eros.tgz
	tar cvf eros.tar Makefile $(TARDIRS)
	gzip eros.tar
	mv eros.tar.gz eros.tgz

pristine: nodepend
	$(MAKE) targdir-clean
	$(MAKE) clean
ifeq "$(NEED_CROSS_BOOTSTRAP)" "yes"
	$(MAKE) cross-bootstrap
endif
	$(MAKE) world

# Following is a very temporary target placed here by shap until I
# can get the test cases cleaned up enough to move this down into
# the test directory where it belongs
regress:
	$(MAKE) -C tutorial/hello-large clean vmware
	$(MAKE) -C tutorial/hello-large clean
	$(MAKE) -C tutorial/triv-keeper clean vmware
	$(MAKE) -C tutorial/triv-keeper clean
	$(MAKE) -C base/tests/func/constructor_test clean vmware
	$(MAKE) -C base/tests/func/constructor_test clean
