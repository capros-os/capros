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

ifndef MAKEVARS_LOADED
include $(EROS_SRC)/build/make/makevars.mk
endif

# The following variables depend on things set in the makefile:
GCCFLAGS=$(CFLAGS) $(OPTIM) $(GCC_OPTIM) $(INC) $(DEF)
GPLUSFLAGS=-fdefault-inline -fno-implicit-templates $(OPTIM) $(GPLUS_OPTIM) $(INC) $(DEF)

DEP_SEDHACK=sed 's,^[^:]*:[:]*,'$@' '$(BUILDDIR)/'&,g'

C_DEP=@$(GCC) $(GCCFLAGS) $(GCCWARN) -E -M $< | $(DEP_SEDHACK) > $@
CXX_DEP=@$(GPLUS) $(GPLUSFLAGS) $(GPLUSWARN) -E -M $< | $(DEP_SEDHACK) > $@
ASM_DEP=@$(GCC) $(GCCFLAGS) -E -M $< | $(DEP_SEDHACK) > $@
MOPS_DEP=$(GCC) $(GCCFLAGS) $(MOPSWARN) -S $< -o $@

C_BUILD=$(GCC) $(GCCFLAGS) $(GCCWARN) -c $< -o $@
CXX_BUILD=$(GPLUS) $(GPLUSFLAGS) $(GPLUSWARN) -c $< -o $@
ASM_BUILD=$(GCC) $(GCCFLAGS) -c $< -o $@
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
DEPEND: $(BUILDDIR)
MAKE_BUILDDIR=$(BUILDDIR)
endif


$(BUILDDIR)/.%.m: %.S $(MAKE_BUILDDIR)
	$(ASM_DEP) #$(GPLUS) $(GCCFLAGS) -E -M $< | $(DEP_SEDHACK) > $@

$(BUILDDIR)/.%.m: %.s $(MAKE_BUILDDIR)
	@echo "Please rewrite .s file as .S file"

$(BUILDDIR)/.%.m: %.c $(MAKE_BUILDDIR)
	$(C_DEP) # $(GCC) $(GCCFLAGS) -E -M $< | $(DEP_SEDHACK) > $@

$(BUILDDIR)/.%.cfg.m: %.c $(MAKE_BUILDDIR)
	$(MOPS_DEP) # $(GCC) $(GCCFLAGS) -E -M $< | $(DEP_SEDHACK) > $@

$(BUILDDIR)/.%.m: %.cxx $(MAKE_BUILDDIR)
	$(CXX_DEP) #$(GPLUS) $(GPLUSFLAGS) $(GPLUSWARN) -E -M $< | $(DEP_SEDHACK) > $@

#
# Object construction rules
#

$(BUILDDIR)/%.o: %.S $(MAKE_BUILDDIR)
	$(ASM_BUILD) 

$(BUILDDIR)/%.o: %.c $(MAKE_BUILDDIR)
	$(C_BUILD) 

$(BUILDDIR)/%.cfg: %.c $(MAKE_BUILDDIR)
	$(MOPS_BUILD) 

$(BUILDDIR)/%.o: %.cxx $(MAKE_BUILDDIR)
	$(CXX_BUILD) 


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
ETAGEXPAND+=$(patsubst %,%/*.cxx,$(ETAGDIRS))
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

# The following hack fixes up directory dependencies, and ALSO ensures
# that the .m files will be rebuilt when appropriate:
#DEPEND: $(patsubst %.o,$(BUILDDIR)/%.m,$(OBJECTS)) 
# For all .o files in $(OBJECTS), the corresponding $(BUILDDIR)/.*.m file
# is a prerequisite to DEPEND.
DEPEND: $(addprefix $(BUILDDIR)/,$(patsubst %.o,.%.m,$(notdir $(OBJECTS))))
DEPEND: $(addprefix $(BUILDDIR)/,$(patsubst %.xml,.%.xml.m,$(wildcard *.xml)))

# When we are building, we need to generate dependencies
install: DEPEND

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
