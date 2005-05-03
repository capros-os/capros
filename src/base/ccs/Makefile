#
# Copyright (C) 2003, The EROS Group, LLC.
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
# Contributor(s):
#

default: world

EROS_SRC=../..
include $(EROS_SRC)/build/make/makevars.mk

include $(EROS_SRC)/build/make/pkgrules.mk

########################################################################
#
# When cross-building, getting the compilers bootstrapped is a major
# pain in the ass because of the interdependency structure. The
# problem in a nutshell is that FSF doesn't know how to maintain
# package structures. Here are the specific problems we run in to:
#
#  1. Certain libraries are duplicated in multiple packages, must
#     notably bfd and libiberty. This results in version drift.
#
#  2. glibc is a separate distribution, and the OLD libstdc++ was
#     a separate package. Unfortunately, beginning with gcc-3.x the
#     libstdc++-v3 stuff is now part of gcc/g++, even though libc
#     still ISN'T part of gcc3.
#
#  3. Some genious at FSF decided that the cross-build process for
#     libc should build libiberty, which depends on cross-libc, but
#     you cannot build cross-libc until you have built cross-gcc!
#
#  4. [EROS Specific] We can't build libc until you have compiled the
#     base IDL files into the corresponding headers, because libc uses
#     some of the generated interfaces.
#
#  5. You can't even *configure* cross-gcc until you have build and
#     installed cross-binutils.
#
#  6. GCC needs to know (compiled in) where the libc headers are to be
#     found. Libc in turn needs cross-gcc in order to build. This
#     means that gcc and libc and kernel and base effectively need to
#     be in the same tree.
#
# Yuck.
#
#
# It seems that the only path out of this mess is to proceed as
# follows:
#
# CROSS-BOOTSTRAP:
#
#     1. Build and install cross-binutils, as this can be done 
#        in completely standalone fashion.
#
#     2. Compile and install cross-gcc (only), carefully suppressing
#        the build of all the helpful shit that FSF wants to
#        cross-build for us. 
#
#     3. Compile and install the "build" package (which contains
#        capidl).
#
#     4. "make interfaces" -- publishes all relevant header files
#
#     5. "make ccs" -- compiles and installs g++, libc, libstdc++. 
#        We need to do libstdc++ now because later libraries may rely
#        on the libstdc++ headers. libstdc++ in turn needs g++, so we
#        might as well build it here.
#
#     6. "make libs" -- builds and installs other libraries.
#
#     7. "make install" -- builds and installs applications
#
# As a result of all of the above, I'm slowly coming to the conclusion
# that the entire notion of package independence was pretty much a
# complete crock.
########################################################################


########################################################################
#
# Stuff beginning here is used in the FIRST pass build of the cross
# compiler. The configure targets configure everything as though we
# were going to do a full cross build. The binutils build actually IS
# a full build, but the first-cross-gcc build is a first-pass build 
# only. The rest of the cross-compiler build will later get completed
# by builting stamp.cross-gcc
#
########################################################################
ifeq "$(EROS_HOSTENV)" "linux-xenv-gcc3"
GCC_VERSION=gcc-3.3
else
GCC_VERSION=gcc-2.8.1
endif
BINUTILS_VERSION=binutils-2.14
NEWLIB_VERSION=newlib-1.11.0

ifeq "$(GCC_VERSION)" "gcc-3.3"
TOOLPATH=$(EROS_ROOT)/host/bin
NEWLIB_CONFIG_DEPS=build/stamp.first-cross-gcc
CROSS_BOOTSTRAP_DEPS=cross-binutils first-cross-gcc
else
TOOLPATH=$(EROS_XENV)/bin
NEWLIB_CONFIG_DEPS=build/stamp.build-dir
CROSS_BOOTSTRAP_DEPS=
endif

cross-bootstrap: $(CROSS_BOOTSTRAP_DEPS)

cross-binutils: build/stamp.cross-binutils

first-cross-gcc: build/stamp.first-cross-gcc

build/stamp.cross-binutils-config: build/stamp.build-dir
	if [ ! -d build/cross-binutils ]; then mkdir build/cross-binutils; fi
	(cd build/cross-binutils; \
	 PATH=$(TOOLPATH):$(PATH) \
	 CC="$(EROS_CCACHE) gcc" \
			../../src/$(BINUTILS_VERSION)/configure \
			--prefix=$(EROS_ROOT)/host \
			--target=$(EROS_TARGET)-unknown-eros)
	touch build/stamp.cross-binutils-config

build/stamp.cross-binutils: build/stamp.cross-binutils-config
	PATH=$(TOOLPATH):$(PATH) \
		$(MAKE) -C build/cross-binutils all install
	touch build/stamp.cross-binutils

build/stamp.cross-gcc-config: build/stamp.cross-binutils
	if [ ! -d build/cross-gcc ]; then mkdir build/cross-gcc; fi
	(cd build/cross-gcc; \
	 PATH=$(TOOLPATH):$(PATH) \
	 CC="$(EROS_CCACHE) gcc" \
			../../src/$(GCC_VERSION)/configure \
			--with-newlib \
			--enable-languages="c c++" \
			--prefix=$(EROS_ROOT)/host \
			--libdir=$(EROS_ROOT)/lib \
			--includedir=$(EROS_ROOT)/include \
			--target=$(EROS_TARGET)-unknown-eros)
	touch build/stamp.cross-gcc-config

build/stamp.first-cross-gcc: build/stamp.cross-gcc-config
	PATH=$(TOOLPATH):$(PATH) \
		$(MAKE) -C build/cross-gcc \
			LANGUAGES="c" \
			CROSS_LIBGCC1="" \
			INSTALL_MODULES="" \
			INSTALL_TARGET_MODULES="" \
			ALL_TARGET_MODULES="" \
			TARGET_CONFIGDIRS="" \
			INSTALL_X11_MODULES="" \
			INSTALL_DOSREL="" \
			all install
	touch build/stamp.first-cross-gcc

build/stamp.newlib-config: $(NEWLIB_CONFIG_DEPS)
	if [ ! -d build/cross-newlib ]; then mkdir build/cross-newlib; fi
	(cd build/cross-newlib; \
	 PATH=$(TOOLPATH):$(PATH) \
			../../src/$(NEWLIB_VERSION)/configure \
			--prefix=$(EROS_ROOT) \
			--infodir=$(EROS_ROOT)/host/info \
			--target=$(EROS_TARGET)-unknown-eros)
	touch build/stamp.newlib-config

build/stamp.newlib: build/stamp.newlib-config
	PATH=$(TOOLPATH):$(PATH) \
		$(MAKE) -C build/cross-newlib STAGE_CC_WRAPPER="$(EROS_CCACHE)" tooldir=$(EROS_ROOT) all install
	touch build/stamp.newlib

build/stamp.build-dir:
	if [ ! -d build ]; then mkdir build; fi
	touch build/stamp.build-dir


newlib: build/stamp.newlib

clean: clean-build

clean-build:
	if [ -d build ]; then rm -rf build; fi

install: newlib
