#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
# Copyright (C) 2006, 2007, Strawberry Development Group.
#
# This file is part of the CapROS Operating System runtime library.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.

# This material is based upon work supported by the US Defense Advanced
# Research Projects Agency under Contract No. W31P4Q-07-C-0070.
# Approved for public release, distribution unlimited.

default: package

EROS_SRC=../../../..

include $(EROS_SRC)/build/make/makevars.mk

BUILDDIR=.
TARGETS=
HEADERS=\
 alternative.h \
 assembler.h \
 atomic.h \
 autoconf.h \
 auxvec.h \
 bitops.h \
 bug.h \
 byteorder.h \
 cache.h \
 cmpxchg.h \
 cpufeature.h \
 cputime.h \
 current.h \
 delay.h \
 desc.h \
 device.h \
 div64.h \
 domain.h \
 elf.h \
 errno.h \
 fcntl.h \
 fpstate.h \
 glue.h \
 hardirq.h \
 hardware.h \
 hw_irq.h \
 io.h \
 ioctl.h \
 ioctls.h \
 ipcbuf.h \
 irq.h \
 irq_regs.h \
 irqflags.h \
 ldt.h \
 linkage.h \
 local.h \
 locks.h \
 math_emu.h \
 memory.h \
 mmu.h \
 module.h \
 msr.h \
 msr-index.h \
 page.h \
 param.h \
 percpu.h \
 posix_types.h \
 processor.h \
 processor-flags.h \
 ptrace.h \
 ptrace-abi.h \
 required-features.h \
 resource.h \
 sections.h \
 segment.h \
 semaphore.h \
 sembuf.h \
 sigcontext.h \
 siginfo.h \
 signal.h \
 sizes.h \
 spinlock.h \
 stat.h \
 string.h \
 system.h \
 termbits.h \
 termios.h \
 thread_info.h \
 timex.h \
 topology.h \
 tsc.h \
 types.h \
 user.h \
 vm86.h \

GENERATED=$(HEADERS)

CLEANLIST += $(GENERATED)

include $(EROS_SRC)/build/make/makerules.mk

GENASMHEADERS=GenAsmHeaders.awk
ARCHLIST=$(EROS_SRC)/sys/ARCH-LIST

.FORCE:

$(GENERATED): $(ARCHLIST) Makefile $(GENASMHEADERS)
	$(GAWK) -f $(GENASMHEADERS) $@ < $(ARCHLIST)

install: $(HEADERS)
	$(INSTALL) -d $(EROS_ROOT)/include/linux-headers/asm/
	$(INSTALL) -m 0644 $(HEADERS) $(EROS_ROOT)/include/linux-headers/asm/
