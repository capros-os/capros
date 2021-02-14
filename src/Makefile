#
# Copyright (C) 2001, The EROS Group, LLC.
# Copyright (C) 2007-2010, Strawberry Development Group.
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

default: packages

EROS_SRC=.

# DIRS are ordered.
IDIRS=sys build base $(CAPROS_LOCALDIR)
LDIRS=build base $(CAPROS_LOCALDIR)
PDIRS=$(IDIRS) legal doc tutorials

TARDIRS=$(PDIRS)

include $(EROS_SRC)/build/make/pkgrules.mk

distclean: targdir-clean clean

targdir-clean:
	-rm -rf $(EROS_ROOT)/include $(EROS_ROOT)/lib $(EROS_ROOT)/idl
	-rm -rf $(EROS_ROOT)/domain $(EROS_ROOT)/doc
	-rm -rf $(EROS_ROOT)/host

IDL_STUB_DIR=$(EROS_SRC)/base/lib/idlstub/$(BUILDDIR)

capidl:
	$(MAKE) -C build/src $(MAKERULES) interfaces

xonly:
	$(MAKE) -C sys $(MAKERULES) install
	$(MAKE) -C base/lib $(MAKERULES) install
	$(MAKE) $(IDL_STUB_DIR)/idlbuilt install IDL_FILE_LIST="`cat $(IDL_DIR)/filelistflat`"
	$(MAKE) -C base/domain $(MAKERULES) install

# Building proceeds in 4 steps:
# 1. Recurse over the source tree gathering .idl and .h files. 
# 2. Build IDL headers and stubs. 
# 3. Recurse over the source tree building libraries.
# 4. Recurse over the source tree building the rest of the files.
packages: capidl idl-list
	$(INSTALL) -d $(IDL_STUB_DIR)
	$(MAKE) $(IDL_STUB_DIR)/idlbuilt IDL_FILE_LIST="`cat $(IDL_DIR)/filelistflat`"
	$(MAKE) libs
	$(MAKE) install

.PHONY: idl-list
idl-list: interfaces
	$(INSTALL) -d $(IDL_DIR)/tmp
	find $(IDL_DIR) -name '*.idl' > $(IDL_DIR)/tmp/filelist
	# Put the list all on one line.
	echo `cat $(IDL_DIR)/tmp/filelist` > $(IDL_DIR)/tmp/filelistflat
	# Copy filelistflat to permanent location if it has changed.
	$(INSTALL) -m 0644 $(IDL_DIR)/tmp/filelistflat $(IDL_DIR)

# Rebuild IDL if any of the .idl files has changed,
# or if the set of files has changed.
$(IDL_STUB_DIR)/idlbuilt: $(IDL_FILE_LIST) $(IDL_DIR)/filelistflat
	# Create headers.
	$(CAPIDL) -D $(EROS_ROOT)/include/idl.tmp -l c-header $(IDL_FILE_LIST)
	$(INSTALL) -d $(EROS_ROOT)/include/idl
	build/bin/dirsync $(EROS_ROOT)/include/idl.tmp $(EROS_ROOT)/include/idl
	$(INSTALL) -d $(EROS_ROOT)/host/include/idl
	build/bin/dirsync $(EROS_ROOT)/include/idl.tmp $(EROS_ROOT)/host/include/idl
	-rm -rf $(EROS_ROOT)/include/idl.tmp
	# Create stubs.
	rm -f $(IDL_STUB_DIR)/*.c
	$(CAPIDL) -D $(IDL_STUB_DIR) -l c-stubs $(IDL_FILE_LIST)
	echo > $@

# just build + install the system headers.
system-headers: idl-list
	$(INSTALL) -d $(IDL_STUB_DIR)
	$(MAKE) $(EROS_ROOT)/include/idl IDL_FILE_LIST="`cat $(IDL_DIR)/filelistflat`"

$(EROS_ROOT)/include/idl: $(IDL_FILE_LIST) $(IDL_DIR)/filelistflat
	# Create headers.
	$(CAPIDL) -D $(EROS_ROOT)/include/idl -l c-header $(IDL_FILE_LIST)
	$(INSTALL) -d $(EROS_ROOT)/include/idl

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

update:
	cvs update -dP
