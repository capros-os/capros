#
# Copyright (C) 2009, Strawberry Development Group
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

default: install

EROS_SRC=../../..
OPENSSL_SOURCE=$(shell pwd)
CROSS_BUILD=yes
include $(EROS_SRC)/build/make/makevars.mk

TARGETS=BUILDSSL/$(TARGETMACH)/libssl.a BUILDSSL/$(TARGETMACH)/libcrypto.a

include $(EROS_SRC)/build/make/makerules.mk

BUILDSSL/$(TARGETMACH):
	@mkdir -p BUILDSSL/$(TARGETMACH)
	cd BUILDSSL/$(TARGETMACH); \
	(cd $(OPENSSL_SOURCE); find . -type f ! -path "./BUILD*" ! -path "./CVS*" ! -name "*.swp") | while read F; do \
		mkdir -p `dirname $$F`; \
		rm -f $$F; ln -s $(OPENSSL_SOURCE)/$$F $$F; \
	done; \
	make -f Makefile.org clean; \
	./Configure no-hw no-threads no-shared no-zlib no-asm no-krb5 CapROS-$(EROS_TARGET)

BUILDSSL/$(TARGETMACH)/libssl.a: BUILDSSL/$(TARGETMACH)
	make -C BUILDSSL/$(TARGETMACH) build_libs

install: BUILDSSL/$(TARGETMACH)/libssl.a
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)

nonrecursiveClean:
	rm -rf BUILDSSL/
