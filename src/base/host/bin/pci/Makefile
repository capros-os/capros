#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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


default: package

EROS_SRC=../../../..
include $(EROS_SRC)/build/make/makevars.mk

CC=gcc
CFLAGS=-g

TARGETS= \
	linux.vendor.list \
	boemler.vendor.list \
	linux.device.list \
	boemler.device.list \
	cklist \
	eros-genhdr

CLEANLIST=boemler.list.c bgenvendor bgendev

depend install: # do nothing
package all: $(TARGETS)

boemler.vendor.list: bgenvendor
	./bgenvendor > boemler.vendor.list

boemler.device.list: bgendev
	./bgendev > boemler.device.list

bgenvendor: boemler.list.o bgenvendor.o
	gcc $(CFLAGS) -o bgenvendor bgenvendor.o

bgendev: boemler.list.o bgendev.o
	gcc $(CFLAGS) -o bgendev bgendev.o

linux.vendor.list:
	sh ./lgenvendor > linux.vendor.list

linux.device.list:
	sh ./lgendev > linux.device.list

boemler.list.o: boemler.list.c

boemler.list.c: boemler.list.base
	sh ./bcleanlist boemler.list.base > boemler.list.c

cklist: cklist.o eros.pci.o
	gcc $(CFLAGS) -o cklist cklist.c eros.pci.c

eros-genhdr: eros-genhdr.o eros.pci.o
	gcc $(CFLAGS) -o eros-genhdr eros-genhdr.o eros.pci.o

tags:
generated:
