#
# Copyright (C) 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, 2007, Strawberry Development Group
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

default: all

CROSS_BUILD=yes

EROS_SRC=../../../..
INC=-I$(BUILDDIR) -I$(CAPROS_HEADERS)/include

include $(EROS_SRC)/build/make/makevars.mk

GENERATED=$(BUILDDIR)/constituents.h
IMGMAP=netsys.map

TARGETS=$(BUILDDIR)/xmit_helper $(BUILDDIR)/recv_helper $(BUILDDIR)/helper \
	$(BUILDDIR)/timeout_agent $(BUILDDIR)/netsys $(BUILDDIR)/enet

IDL=$(wildcard *.idl)
NETSYSSRC=netsys.c	\
	  timer.c	\
	  mem.c		\
	  ip.c		\
	  inet.c	\
	  memp.c	\
	  pstore.c      \
	  udp.c		\
	  dhcp.c	\
	  tcp_in.c      \
	  tcp.c         \
	  icmp.c	\
	  etharp.c	\
	  TxRxqueue.c   \
	  tcp_out.c     \
	  Session.c	\
	  ip_frag.c     \
	  netif/netif.c \
	  $(wildcard memmgr/*.c)
NETSYSOBJ=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(NETSYSSRC)))

NETXMIT=xmit_helper.c
NETXMITOBJ=$(BUILDDIR)/xmit_helper.o

NETRECV=recv_helper.c
NETRECVOBJ=$(BUILDDIR)/recv_helper.o

HELPERSRC=netif/helper.c
HELPEROBJ=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(HELPERSRC)))

ENETSRC=netif/enet.c  \
	netif/netutils.c    \
	netif/enet_map.c \
	netif/demux.c \
	netif/enet_session.c \
	netif/lance.c \
	netif/tg3.c \
	netif/_3c905c.c \
	netif/enet_queue.c \
	pstore.c \
	netif/netif.c 
ENETOBJ=$(patsubst %.c,$(BUILDDIR)/%.o,$(notdir $(ENETSRC)))

TIMERSRC=timeout_agent.c
TIMEROBJ=$(BUILDDIR)/timeout_agent.o

include $(EROS_SRC)/build/make/makerules.mk

INC += -I.

OPTIM=-O

LIBS=-lforwarder -laddrspace -lethread

all: $(TARGETS)

nonrecursiveClean: local-clean
local-clean:
	-rm -fr *~ netif/*~ include/*~

interfaces: $(IDL)
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/domain
	$(INSTALL) -d $(EROS_ROOT)/idl/capros/net/shared_ipv4
	$(INSTALL) -m 644 $(IDL) $(EROS_ROOT)/idl/capros/net/shared_ipv4


$(NETSYSSRC): $(BUILDDIR)/constituents.h
xmit_helper.c:$(BUILDDIR)/constituents.h
recv_helper.c:$(BUILDDIR)/constituents.h
netif/helper.c:$(BUILDDIR)/constituents.h
netif/enet.c:$(BUILDDIR)/constituents.h
timeout_agent.c:$(BUILDDIR)/constituents.h

$(BUILDDIR)/netsys: $(NETSYSOBJ) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(NETSYSOBJ) $(LIBS) \
	$(DOMLIB) 

# The two helpers for the netsys domain
$(BUILDDIR)/xmit_helper: $(NETXMITOBJ) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(NETXMITOBJ)	$(SMALL_SPACE) $(CROSSLIBS)
$(BUILDDIR)/recv_helper: $(NETRECVOBJ) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(NETRECVOBJ) $(SMALL_SPACE) $(CROSSLIBS) 


$(BUILDDIR)/helper: $(HELPEROBJ) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(HELPEROBJ) $(SMALL_SPACE) $(CROSSLIBS) 

$(BUILDDIR)/enet: $(ENETOBJ) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(ENETOBJ) $(LIBS) $(CROSSLIBS) 

$(BUILDDIR)/timeout_agent: $(TIMEROBJ) $(DOMLIB) $(DOMCRT0)
	$(CROSSLINK) -o $@ $(TIMEROBJ) $(SMALL_SPACE) $(CROSSLIBS) 

$(BUILDDIR)/%.o: netif/%.c
	$(C_BUILD) $(INC)

$(BUILDDIR)/%.o: memmgr/%.c
	$(C_BUILD) $(INC)


install: all
	$(INSTALL) -d $(CAPROS_DOMAIN)/net/shared_ipv4
	$(INSTALL) -m 755 $(TARGETS) $(CAPROS_DOMAIN)/net/shared_ipv4
	$(INSTALL) -m 644 netsys.map $(CAPROS_DOMAIN)/net/shared_ipv4

-include $(BUILDDIR)/.*.m
