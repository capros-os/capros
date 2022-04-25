#
# Copyright (C) 2003, Jonathan S. Shapiro.
# Copyright (C) 2005-2010, Strawberry Development Group
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

ifndef MAKEVARS_LOADED
include $(EROS_SRC)/build/make/makevars.mk
endif

# The following variables depend on things set in the makefile:
GCCFLAGS=$(CFLAGS) $(GCC_OPTIM) $(OPTIM) $(INC) -DEROS_TARGET_$(EROS_TARGET) -DCAPROS_MACH_$(CAPROS_MACH) $(DEF)
GPLUSFLAGS=-fdefault-inline $(GPLUS_OPTIM) $(OPTIM) $(INC) -DEROS_TARGET_$(EROS_TARGET) $(DEF)
# The following does not work for capidl:
# GPLUSFLAGS+= -fno-implicit-templates
MKIMAGEFLAGS=-a $(EROS_TARGET) -DBUILDDIR='"$(BUILDDIR)/"' -DEROS_TARGET_$(EROS_TARGET) -DLIBDIR=\"$(CAPROS_DOMAIN)/\" -DCAPROS_LOCALDIR=$(CAPROS_LOCALDIR) -I$(CAPROS_DOMAIN) -I$(EROS_ROOT)/host/include $(LINUXINC)

# __ASSEMBLER__ gets defined automatically, but Linux requires:
ASMFLAGS=-D__ASSEMBLY__

C_DEP=@$(GCC) $(GCCFLAGS) $(GCCWARN) -M -MT $@ -MF $(BUILDDIR)/.$(patsubst %.o,%.m,$(notdir $@)) $<
CPP_DEP=@$(GPLUS) $(GPLUSFLAGS) $(GPLUSWARN) -M -MT $@ -MF $(BUILDDIR)/.$(patsubst %.o,%.m,$(notdir $@)) $<
ASM_DEP=@$(GCC) $(GCCFLAGS) $(ASMFLAGS) -M -MT $@ -MF $(BUILDDIR)/.$(patsubst %.o,%.m,$(notdir $@)) $<
#MOPS_DEP=$(GCC) $(GCCFLAGS) $(MOPSWARN) -S $< -o $(BUILDDIR)/.$(patsubst %.o,%.m,$(notdir $@))

C_BUILD=$(GCC) $(GCCFLAGS) $(GCCWARN) -c $< -o $@
CPP_BUILD=$(GPLUS) $(GPLUSFLAGS) $(GPLUSWARN) -c $< -o $@
ASM_BUILD=$(GCC) $(GCCFLAGS) $(ASMFLAGS) -c $< -o $@
MOPS_BUILD=$(GCC) -B$(MOPS)/rc/ $(GCCFLAGS) $(MOPSWARN) -S $< -o $@

$(BUILDDIR):
	if [ ! -d $(BUILDDIR) ]; then \
		mkdir -p $(BUILDDIR); \
	fi
######################################################################
#
# Only want to rebuild the build directory if it doesn't already exist.
# Cannot list $(BUILDDIR) as a dependency directly, because the directory
# timestamp changes every time we compile, which forces rebuilds
# unnecessarily.
#
######################################################################

ifeq "$(wildcard $(BUILDDIR))" ""
MAKE_BUILDDIR=$(BUILDDIR)
endif

#
# Object construction rules
#

$(BUILDDIR)/%.o: %.S $(MAKE_BUILDDIR)
	$(ASM_BUILD) 
	$(ASM_DEP)

$(BUILDDIR)/%.o: %.c $(MAKE_BUILDDIR)
	$(C_BUILD) 
	$(C_DEP) 

$(BUILDDIR)/%.cfg: %.c $(MAKE_BUILDDIR)
	$(MOPS_BUILD) 
	$(MOPS_DEP)

$(BUILDDIR)/%.o: %.cpp $(MAKE_BUILDDIR)
	$(CPP_BUILD) 
	$(CPP_DEP)


# Rules to process XML files to produce HTML:
%.html: %.xml
	$(XSLT) -xinclude --param docname \"$*\" -o $@ $(EROS_SRC)/doc/www/DTD/html-doc.xslt $<

$(BUILDDIR)/%.html: %.xml $(MAKE_BUILDDIR)
	$(XSLT) -xinclude --param docname \"$*\" -o $@ $(EROS_SRC)/doc/www/DTD/html-doc.xslt $<

.%.xml.m: %.xml
	@echo "$(<:.xml=.html): $< $(EROS_SRC)/doc/www/DTD/html-doc.xslt" > $@

$(BUILDDIR)/.%.xml.m: %.xml $(MAKE_BUILDDIR)
	@echo "$(<:.xml=.html): $< $(EROS_SRC)/doc/www/DTD/html-doc.xslt" > $@

%.dvi: %.ltx
	latex $<
	latex $<

%.gif: %.fig
	@if [ \! -x $(NETPBMDIR)/ppmtogif ]; then\
		echo "ERROR: Please set the NETPBMDIR environment variable to identify"; \
		echo "       the location of the NetPBM directory."; \
		exit 1;\
	fi
	fig2dev -L ppm $< $*.ppm
	$(NETPBMDIR)/ppmtogif -t '#ffffff' $*.ppm > $@
	-rm -f $*.ppm

ifdef ETAGDIRS
ETAGEXPAND=$(patsubst %,%/*.c,$(ETAGDIRS))
ETAGEXPAND+=$(patsubst %,%/*.cpp,$(ETAGDIRS))
ETAGEXPAND+=$(patsubst %,%/*.hxx,$(ETAGDIRS))
ETAGEXPAND+=$(patsubst %,%/*.h,$(ETAGDIRS))

ASM_ETAGEXPAND+=$(patsubst %,%/*.S,$(ETAGDIRS))

ETAGFILES=$(wildcard $(ETAGEXPAND))
ASM_ETAGFILES=$(wildcard $(ASM_ETAGEXPAND))
unexport ETAGEXPAND
unexport ETAGFILES
unexport ASM_ETAGEXPAND
unexport ASM_ETAGFILES

#	--regex='/^struct[ \t]+\([A-Za-z0-9_]+\)[ \t]*:[ \t]*public[ \t]+\([A-Za-z0-9_]+\)/:public \1 \2/' \
#	--regex='/^class[ \t]+\([A-Za-z0-9_]+\)[ \t]*:[ \t]*public[ \t]+\([A-Za-z0-9_]+\)/:public \1 \2/' \

tags:
	etags \
	--regex='/^struct[ \t]+\([A-Za-z0-9_]+\)[ \t]*:[ \t]*public[ \t]+\([A-Za-z0-9_]+\)/:public \1 \2/' \
	--regex='/^class[ \t]+\([A-Za-z0-9_]+\)[ \t]*:[ \t]*public[ \t]+\([A-Za-z0-9_]+\)/:public \1 \2/' \
	   $(ETAGFILES)
ifneq ($(ASM_ETAGFILES),)
	etags --append --lang=none \
		--regex='/^\(ENTRY\|GEXT\)([A-Za-z0-9_\.]+)/' \
		$(ASM_ETAGFILES)
endif
endif

# Imagemap files may have lines defining constructor constituents.
# This makes a C header file defining the constituent slot numbers.
$(BUILDDIR)/constituents.h: $(IMGMAP) $(MAKE_BUILDDIR)
	@grep 'CONSTIT(' $(IMGMAP) | \
		grep -v '#define' | \
		sed 's/[ ]*=.*$$//' | \
		sed 's/^[^,]*, */#define /' | \
		sed 's/)[^)]*$$//' | \
		sed 's/,/ /' > $@
	@echo "Making $@"

BUILDRULES_LOADED=1
