#
# Copyright (C) 1998, 1999, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

GOOD_TARGET=0
ifndef EROS_TARGET
EROS_TARGET=i486
endif
ifeq ($(EROS_TARGET),i486)
GOOD_TARGET=1
endif
ifeq ($(EROS_TARGET),arm)
GOOD_TARGET=1
ifndef CAPROS_MACH
CAPROS_MACH=edb9315
endif
endif
ifneq ($(GOOD_TARGET),1)
$(error "arm" and "i486" are the only supported values of EROS_TARGET)
endif

ifndef CAPROS_MACH
CAPROS_MACH=generic
endif

# Sometimes we want i486 (for compatibility with EROS stuff),
# sometimes i386 (for compatibility with Linux)
ifeq "$(EROS_TARGET)" "i486"
LINUX_TARGET=i386
else
LINUX_TARGET=$(EROS_TARGET)
endif

ifndef EROS_ROOT
endif
ifndef EROS_XENV
EROS_XENV=/capros/host
endif
ifndef EROS_CONFIG
EROS_CONFIG=DEFAULT
endif

CAPROS_DOMAIN=$(EROS_ROOT)/lib/$(TARGETMACH)

VMWARE=$(EROS_ROOT)/src/build/bin/vmdbg
export EROS_ROOT
export EROS_TARGET
export EROS_XENV
export EROS_CONFIG

INSTALL=$(EROS_SRC)/build/bin/erosinstall
REPLACE=$(EROS_SRC)/build/bin/move-if-change
MKIMAGE=$(EROS_ROOT)/host/bin/$(EROS_TARGET)-mkimage
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
#HTMLCAPIDL = $(EROS_SRC)/build/bin/coyotos-capidl
#HTMLCAPIDL = /home/clandau/coyotos/src/ccs/capidl/BUILD/capidl

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

ifeq "$(EROS_CONFIG)" "NDEBUG"
EROS_GCC_OPTIM+= -O2
else
EROS_GCC_OPTIM+= -O1
ifeq "$(EROS_TARGET)" "arm"
EROS_GCC_OPTIM+= -mapcs-frame # generate stack frames for debugging
endif
endif

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


# Libraries for make dependencies.
LIBDEP=$(CAPROS_DOMAIN)/libc-capros.a
LIBDEP+=$(CAPROS_DOMAIN)/libcapros-large.a
LIBDEP+=$(CAPROS_DOMAIN)/libcapros-small.a
DOMLIB=$(LIBDEP)	# an older name

# DOMCRT0 is obsolete, but still appears in some make dependency lists.
DOMCRT0=

# DOMBASE could be zero, but this value helps catch use of NULL pointers
# by both user code and kernel code.
DOMBASE=0x1000

LINKOPT=-Wl,--section-start,.init=$(DOMBASE) -static -L$(CAPROS_DOMAIN) -e _start #-Wl,--verbose -v
DOMLINKOPT=$(LINKOPT)

CROSSLINK=$(EROS_GCC) $(DOMLINKOPT) #-v

CMTESTART=$(CAPROS_DOMAIN)/cmtestart.o
# Put the read/write section at 0x00c00000:
CMTELINKOPT=$(LINKOPT) -Wl,--section-start,.eh_frame=0x00c00000
CMTELINK=$(EROS_GCC) $(CMTELINKOPT) $(CMTESTART)
# libs given at the end of the link command:
CMTELIBS=$(CAPROS_DOMAIN)/libcmte.a
CMTEDEPS=$(CMTELIBS) $(CMTESTART) $(LIBDEP)

DEVSTART=$(CAPROS_DOMAIN)/dstart.o
DRIVERLINK=$(CMTELINK) $(DEVSTART)
LINUXLIB=$(CAPROS_DOMAIN)/liblinuxk.a
DRIVERLIBS=$(CMTELIBS) $(LINUXLIB)
DRIVERDEPS=$(CMTEDEPS) $(DEVSTART) $(LINUXLIB)

DYNCMTESTART=$(CAPROS_DOMAIN)/dyncmtestart.o
DYNCMTELINK=$(EROS_GCC) $(CMTELINKOPT) $(DYNCMTESTART)
DYNCMTEDEPS=$(LIBDEP) $(CMTELIBS) $(DYNCMTESTART)

DYNDRVSTART=$(CAPROS_DOMAIN)/dyndriverstart.o
DYNDRIVERLINK=$(DYNCMTELINK) $(DYNDRVSTART)
DYNDRIVERDEPS=$(DYNCMTEDEPS) $(DYNDRVSTART)

CROSSLIBS=
SMALL_SPACE=-small-space


# Really ugly GNU Makeism to extract the name of the current package by
# stripping $EROS_ROOT/ out of $PWD (yields: src/PKG/xxxxxx), turning /
# characters into spaces (yields: "src PKG xxx xxx xxx"), and  
# then selecting the appropriate word. Notice that the first substring is 
# empty, so the appropriate word is the second one.
PACKAGE=$(word 1, $(strip $(subst /, ,$(subst $(EROS_ROOT)/src/,,$(PWD)))))

TARGETMACH=$(EROS_TARGET)-$(CAPROS_MACH)

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
BUILDDIR=BUILD/$(TARGETMACH)
endif
endif

showme:
	@echo "EROS_ROOT: " $(EROS_ROOT)
	@echo "EROS_SRCDIR: " $(EROS_SRCDIR)
	@echo "EROS_XENV: " $(EROS_XENV)
	@echo "EROS_TARGET: " $(EROS_TARGET)
	@echo "CAPROS_MACH: " $(CAPROS_MACH)
	@echo "PKG_ROOT:" $(PKG_ROOT)
	@echo "PKG_SRC:" $(PKG_SRC)
	@echo "BUILDDIR:" $(BUILDDIR)

MAKEVARS_LOADED=1
