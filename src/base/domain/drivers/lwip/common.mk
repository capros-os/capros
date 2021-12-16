#
# Copyright (C) 2008-2010, Strawberry Development Group.
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

# Outer Makefile must define LWIP_ROOT.

VPATH=$(LWIP_ROOT)
VPATH+=$(LWIP_ROOT)/core
VPATH+=:$(LWIP_ROOT)/core/ipv4
VPATH+=:$(LWIP_ROOT)/netif
VPATH+=:$(LWIP_ROOT)/arch

COMMON_OBJECTS=
COMMON_OBJECTS+= $(BUILDDIR)/cap.o
COMMON_OBJECTS+= $(BUILDDIR)/capudp.o
# from core:
COMMON_OBJECTS+= $(BUILDDIR)/dhcp.o
COMMON_OBJECTS+= $(BUILDDIR)/dns.o
COMMON_OBJECTS+= $(BUILDDIR)/init.o
COMMON_OBJECTS+= $(BUILDDIR)/mem.o
COMMON_OBJECTS+= $(BUILDDIR)/memp.o
COMMON_OBJECTS+= $(BUILDDIR)/netif.o
COMMON_OBJECTS+= $(BUILDDIR)/pbuf.o
COMMON_OBJECTS+= $(BUILDDIR)/raw.o
COMMON_OBJECTS+= $(BUILDDIR)/stats.o
COMMON_OBJECTS+= $(BUILDDIR)/sys.o
COMMON_OBJECTS+= $(BUILDDIR)/tcp.o
COMMON_OBJECTS+= $(BUILDDIR)/tcp_in.o
COMMON_OBJECTS+= $(BUILDDIR)/tcp_out.o
COMMON_OBJECTS+= $(BUILDDIR)/udp.o
# from core/ipv4:
COMMON_OBJECTS+= $(BUILDDIR)/autoip.o
COMMON_OBJECTS+= $(BUILDDIR)/icmp.o
COMMON_OBJECTS+= $(BUILDDIR)/igmp.o
COMMON_OBJECTS+= $(BUILDDIR)/inet.o
COMMON_OBJECTS+= $(BUILDDIR)/inet_chksum.o
COMMON_OBJECTS+= $(BUILDDIR)/ip_addr.o
COMMON_OBJECTS+= $(BUILDDIR)/ip.o
COMMON_OBJECTS+= $(BUILDDIR)/ip_frag.o
# from netif:
COMMON_OBJECTS+= $(BUILDDIR)/etharp.o

INC=-I$(LWIP_ROOT)/include -I $(LWIP_ROOT)/include/ipv4
INC+=$(LINUXINC)
INC+=-I$(EROS_ROOT)/host/include	# for disk/NPODescr.h
INC+= -I$(LWIP_ROOT)/include/arch/$(LINUX2624_TARGET)	# for arch/cc.h
