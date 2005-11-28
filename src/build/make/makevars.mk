#
# Copyright (C) 1998, 1999, Jonathan S. Shapiro.
# Copyright (C) 2005, Strawberry Development Group.
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

# Cancel the old suffix list, because the order matters.  We want assembly 
# source to be recognized in preference to ordinary source code, so the
# .S, .s cases must come ahead of the rest.
.SUFFIXES:
.SUFFIXES: .S .s .cxx .c .y .l .o .dvi .ltx .gif .fig .xml .html .pdf

# Some shells do not export this variable. It is used in the package
# generation rule and in the discovery of EROS_ROOT.
PWD=$(shell pwd)

#
# Set up default values for these variables so that a build in an improperly
# configured environment has a fighting chance:
#
ifdef EROS_ROOT

EROS_SRCDIR=$(word 1, $(strip $(subst /, ,$(subst $(EROS_ROOT)/,,$(PWD)))))
EROS_FINDDIR=old

else

ifneq "" "$(findstring /eros/pkgsrc,$(PWD))"
EROS_SRCDIR=pkgsrc
else
EROS_SRCDIR=src
endif

EROS_ROOT=$(firstword $(subst /eros/$(EROS_SRCDIR), ,$(PWD)))/eros

endif

ifndef EROS_TARGET
EROS_TARGET=i486
endif

ifndef EROS_ROOT
endif
ifndef EROS_XENV
# EROS_XENV=$(HOME)/eros-xenv
EROS_XENV=/capros/host
endif
ifndef EROS_CONFIG
EROS_CONFIG=DEFAULT
endif

VMWARE=$(EROS_ROOT)/src/build/bin/vmdbg
export EROS_ROOT
export EROS_TARGET
export EROS_XENV
export EROS_CONFIG

INSTALL=$(EROS_SRC)/build/bin/erosinstall
REPLACE=$(EROS_SRC)/build/bin/move-if-change
MKIMAGEDEP=$(EROS_SRC)/build/bin/mkimagedep

#
# First, set up variables for building native tools:
#
GAWK=gawk
PYTHON=python
#XML_LINT=$(EROS_XENV)/bin/xmllint
#XSLT=$(EROS_XENV)/bin/xsltproc
XML_LINT=xmllint
XSLT=xsltproc

NATIVE_STRIP=strip
NATIVE_OBJDUMP=objdump
NATIVE_SIZE=size
NATIVE_AR=ar
NATIVE_LD=ld
NATIVE_REAL_LD=ld
NATIVE_RANLIB=ranlib

NATIVE_GCC=gcc
ifndef GCCWARN
MOPSWARN=-Wall -Winline -Wno-format -Wno-char-subscripts
GCCWARN=$(MOPSWARN) -Werror
endif

NATIVE_GPLUS=g++
ifndef GPLUSWARN
GPLUSWARN=-Wall -Winline -Werror -Wno-format
endif

#
# Then variables for building EROS binaries:
#
EROS_GCC=$(NATIVE_GCC)
EROS_GPLUS=$(NATIVE_GPLUS)
EROS_LD=$(NATIVE_LD)
EROS_REAL_LD=$(NATIVE_LD)
EROS_AR=$(NATIVE_AR)
EROS_RANLIB=$(NATIVE_RANLIB)
EROS_OBJDUMP=$(NATIVE_OBJDUMP)
EROS_STRIP=$(NATIVE_STRIP)
EROS_SIZE=$(NATIVE_SIZE)

#
# Then variables related to installation and test generation:
#

HOST_FD=/dev/fd0H1440 

###############################################################
#
# EROS_HD is the disk device that will have a CapROS system
# written to it.
# CAPROS_BOOT_PARTITION is the name of a partition. Files used by GRUB
# to boot the system will be located here. 
#
# DANGER, WILL ROBINSON!!!!
#
# The EROS_HD environment variable is defined to something
# harmless here **intentionally**.  There are too many ways
# to do grievous injuries to a misconfigured host environment
# by setting a default.
#
# It is intended that the intrepid UNIX-based developer should
# pick a hard disk partition, set that up with GRUB,
# make it mode 666 from their UNIX environment, and
# then set EROS_HD to name that partition device file.  This
# is how *I* work, but you do this at your own risk!!
#
###############################################################
ifndef EROS_HD
EROS_HD=/dev/null
CAPROS_BOOT_PARTITION=/boot
endif

CAPIDL=$(EROS_SRC)/build/bin/capidl

#
# This is where the target environment makefile gets a chance to override
# things:
#
ifndef EROS_HOSTENV
EROS_HOSTENV=linux-xenv
endif

include $(EROS_SRC)/build/make/$(EROS_HOSTENV).mk

# search for ppmtogif in all the obvious places:
ifndef NETPBMDIR
ifneq "" "$(findstring /usr/bin/ppmtogif,$(wildcard /usr/bin/*))"
NETPBMDIR=/usr/bin
endif
endif

ifndef NETPBMDIR
ifneq "" "$(findstring /usr/bin/X11/ppmtogif,$(wildcard /usr/bin/X11/*))"
NETPBMDIR=/usr/bin/X11
endif
endif

ifndef NETPBMDIR
ifneq "" "$(findstring /usr/local/netpbm/ppmtogif,$(wildcard /usr/local/netpbm/*))"
NETPBMDIR=/usr/local/netpbm
endif
endif

ifndef EROS_FD
EROS_FD=$(HOST_FD)
endif

#
# Now for the REALLY SLEAZY part: if this makefile is performing a
# cross build, smash the native tool variables with the cross tool 
# variables.  The clean thing to do would be to separate the rules 
# somehow, but this is quicker:
ifdef CROSS_BUILD
GCC=$(EROS_CCACHE) $(EROS_GCC)
GPLUS=$(EROS_CCACHE) $(EROS_GPLUS)
LD=$(EROS_LD)
AR=$(EROS_AR)
OBJDUMP=$(EROS_OBJDUMP)
SIZE=$(EROS_SIZE)
STRIP=$(EROS_STRIP)
RANLIB=$(EROS_RANLIB)
GPLUS_OPTIM=$(EROS_GPLUS_OPTIM)
GCC_OPTIM=$(EROS_GCC_OPTIM)
endif
ifndef CROSS_BUILD
GCC=$(EROS_CCACHE) $(NATIVE_GCC)
GPLUS=$(EROS_CCACHE) $(NATIVE_GPLUS)
LD=$(NATIVE_LD)
AR=$(NATIVE_AR)
OBJDUMP=$(NATIVE_OBJDUMP)
SIZE=$(NATIVE_SIZE)
STRIP=$(NATIVE_STRIP)
RANLIB=$(NATIVE_RANLIB)
GPLUS_OPTIM=$(NATIVE_GPLUS_OPTIM)
GCC_OPTIM=$(NATIVE_GCC_OPTIM)
endif

#
# in all the places where this is run, we actually want the environmental
# definitions set for the target environment.
#

ifndef EROS_CPP
EROS_CPP=$(EROS_XENV)/bin/cpp -nostdinc -D$(EROS_TARGET)
endif


DOMLIB= $(EROS_ROOT)/lib/libdomain.a
DOMLIB += $(EROS_ROOT)/lib/libidlstub.a
DOMLIB += $(EROS_ROOT)/lib/libdomgcc.a
DOMLIB += -lc # libc.a

ifeq "$(EROS_HOSTENV)" "linux-xenv-gcc3"
#DOMCRT0=$(EROS_ROOT)/lib/gcc-lib/$(EROS_TARGET)-unknown-eros/3.3/crt1.o
#DOMCRTN=$(EROS_ROOT)/lib/gcc-lib/$(EROS_TARGET)-unknown-eros/3.3/crtn.o
DOMLINKOPT=-N -static -Ttext 0x0 -L$(EROS_ROOT)/lib
DOMLINK=$(GCC)
else
DOMCRT0=$(EROS_ROOT)/lib/crt0.o
DOMCRTN=$(EROS_ROOT)/lib/crtn.o
DOMSBRK=$(EROS_ROOT)/lib/sbrk.o
# DOMLINKOPT=-N -Ttext 0x0 -nostdlib -static -e _start -L$(EROS_ROOT)/lib -L$(EROS_ROOT)/lib/$(EROS_TARGET)
DOMLINKOPT=-N -Ttext 0x0 -static -e _start -L$(EROS_ROOT)/lib -L$(EROS_ROOT)/lib/$(EROS_TARGET)
DOMLINK=$(EROS_LD)
endif

DOMLIB += $(DOMCRTN)


# Really ugly GNU Makeism to extract the name of the current package by
# stripping $EROS_ROOT/ out of $PWD (yields: src/PKG/xxxxxx), turning /
# characters into spaces (yields: "src PKG xxx xxx xxx"), and  
# then selecting the appropriate word. Notice that the first substring is 
# empty, so the appropriate word is the second one.
PACKAGE=$(word 1, $(strip $(subst /, ,$(subst $(EROS_ROOT)/src/,,$(PWD)))))

PKG_ROOT=$(EROS_ROOT)/pkg/${PACKAGE}
PKG_SRC=$(EROS_ROOT)/$(EROS_SRCDIR)/${PACKAGE}

# Until proven otherwise...
IMGMAP=imgmap

# Until proven otherwise...
BOOTSTRAP=boot

# Until proven otherwise...

ifeq "$(BUILDDIR)" ""
ifeq "$(PACKAGE)" "doc"
BUILDDIR=.
endif
ifeq "$(PACKAGE)" "legal"
BUILDDIR=.
endif
ifeq "$(PACKAGE)" "build"
BUILDDIR=.
endif
ifeq "$(BUILDDIR)" ""
BUILDDIR=BUILD/$(EROS_TARGET)
endif
endif

showme:
	@echo "EROS_ROOT: " $(EROS_ROOT)
	@echo "EROS_SRCDIR: " $(EROS_SRCDIR)
	@echo "EROS_XENV: " $(EROS_XENV)
	@echo "EROS_TARGET: " $(EROS_TARGET)
	@echo "PKG_ROOT:" $(PKG_ROOT)
	@echo "PKG_SRC:" $(PKG_SRC)
	@echo "BUILDDIR:" $(BUILDDIR)

MAKEVARS_LOADED=1
