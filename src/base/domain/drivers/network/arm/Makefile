#
# Copyright (C) 2008, 2009, Strawberry Development Group.
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

CROSS_BUILD=yes

EROS_SRC=../../../../..
include $(EROS_SRC)/build/make/makevars.mk
INC=-I$(BUILDDIR) $(DRIVERINC)

TARGETS=$(BUILDDIR)/ep93xx_eth
OBJECTS=$(BUILDDIR)/ep93xx_eth.o
OBJECTS+=../core/$(BUILDDIR)/dev.o
OBJECTS+=../core/$(BUILDDIR)/eth.o
OBJECTS+=../core/$(BUILDDIR)/link_watch.o
OBJECTS+=../core/$(BUILDDIR)/mii.o
OBJECTS+=../core/$(BUILDDIR)/neighbour.o
OBJECTS+=../core/$(BUILDDIR)/rtnetlink.o
OBJECTS+=../core/$(BUILDDIR)/skbuff.o
OBJECTS+=../core/$(BUILDDIR)/sock.o
OBJECTS+=../core/$(BUILDDIR)/stubs.o ##
OBJECTS+=../core/$(BUILDDIR)/utils.o
OBJECTS+=../ipv4/$(BUILDDIR)/af_inet.o
OBJECTS+=../ipv4/$(BUILDDIR)/arp.o
OBJECTS+=../ipv4/$(BUILDDIR)/tcp.o
OBJECTS+=../ipv4/$(BUILDDIR)/tcp_ipv4.o
OBJECTS+=../netlink/$(BUILDDIR)/attr.o
OBJECTS+=../sched/$(BUILDDIR)/sch_generic.o
IMGMAP=net.map

include $(EROS_SRC)/build/make/makerules.mk

OPTIM=-O

all: $(TARGETS)

$(BUILDDIR)/ep93xx_eth: $(OBJECTS) $(DRIVERDEPS)
	$(DRIVERLINK) $(OBJECTS) $(DRIVERLIBS) -o $@
	$(EROS_OBJDUMP) --disassemble $@ >$(BUILDDIR)/disassembled

install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/drivers/network
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/drivers/network
	$(INSTALL) -m 644 net.map $(CAPROS_DOMAIN)/drivers/network

-include $(BUILDDIR)/.*.m
