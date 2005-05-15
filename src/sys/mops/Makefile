#
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

EROS_CONFIG=DEFAULT

#JPROF=-Xprof
#GC=-verbose:gc
JAVAMEM=1536M
JAVA=java $(GC) $(JPROF) -Xms$(JAVAMEM) -Xmx$(JAVAMEM) -cp :$(MOPS)/src/class:$(MOPS)/lib/java-getopt-1.0.9.jar
COMPACT = $(JAVA) CfgCompact
MERGE = $(JAVA) CfgMerge
CHECK = $(JAVA) Check
TRANSFORM = $(JAVA) Transform

MERGED_CFG=../arch/i486/BUILD/$(EROS_CONFIG)/$(EROS_CONFIG).eros.cfg
CFG=$(MERGED_CFG)

# These traces get made by default:
TRACES=sleep.tra YieldOrCommit.tra irqyield.tra GetRegs.tra keynhset.tra pteset.tra
sleep.tra: FN=-f entryfunction.txt

YieldOrCommit.tra: FN=-e proc_DoGeneralKeyInvocation

irqyield.tra: FN=-f entryfunction.txt

GetRegs.tra: FN=-e ProcessKey

keynhset.tra: FN=-e proc_DoKeyInvocation

pteset.tra: FN=-e proc_DoPageFault

.PHONY: all traces clean

all: traces

ifeq ("$(MOPS)","")
traces:
	@echo "MOPS does not appear to be installed.  Please specify the MOPS environment variable."
	@echo "  e.g. export MOPS=/home/hchen/mops"
else
traces: $(TRACES)
endif

CLEANLIST=*.scfg *.cfg *.stra *.tra *.tra.[0-9]* *.tra.trimmed MERGED-CFG.txt

CFG.txt: $(CFG)
	$(JAVA) Bin2Text $(CFG) CFG.txt


#FN=-f entryfunction.txt

#YieldOrCommit: FN=-e proc_DoGeneralKeyInvocation
#YieldOrCommit: YieldOrCommit.showme




#	for i in YieldOrCommit.tra.[0-9]*; do \
#		python trimtrace2.py $$i; \
#	done

ifeq "$(wildcard $(TRA).[0-9]*)" ""
ShowMe:
	@echo No errors found
else
ShowMe:
	cat $(wildcard $(TRA).[0-9]*)
endif

.PRECIOUS: %.scfg %.stra %.tra

%.scfg : $(MERGED_CFG)
	$(COMPACT) -m $*.mfsa -i $(MERGED_CFG) $(FN) -o $@

%.stra : %.scfg %.mfsa
	$(CHECK) -m $*.mfsa -i $*.scfg $(FN) -o $@

%.tra: %.stra $(MERGED_CFG)
	-rm -rf $@.[0-9]*
	$(TRANSFORM) -c $(MERGED_CFG) -i $*.stra -o $@

%.showme: %.tra
	$(MAKE) TRA=$< ShowMe
