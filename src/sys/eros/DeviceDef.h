#ifndef __DEVICEDEFS_H__
#define __DEVICEDEFS_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */


/*  This interface is based on:
   
    PCI BIOS Specification Revision  v 2.1
    PCI Local Bus Specification      v 2.1
    
    PCI Special Interest Group
    M/S HF3-15A
    5200 N.E. Elam Young Parkway
    Hillsboro, Oregon 97124-6497
    +1 (503) 696-2000 
    +1 (800) 433-5177 */

# define DEV_GET_CLASS(dc) ((dc >> 8) & 0xffu)
# define DEV_GET_SUBCLASS(dc) (dc & 0xffu)

# define DEVCLASS(cls, sub)  ((cls << 8) | sub)

# define DEV_ANCIENT_NONVGA    DEVCLASS(0x0, 0x0)
# define DEV_ANCIENT_VGA       DEVCLASS(0x0, 0x1)

# define DEV_DISK_OPTION_SCSI         DEVCLASS(0x1, 0x0)
# define DEV_DISK_IDE          DEVCLASS(0x1, 0x1)
# define DEV_DISK_FLOPPY       DEVCLASS(0x1, 0x2)
# define DEV_DISK_IPI          DEVCLASS(0x1, 0x3)
# define DEV_DISK_RAID         DEVCLASS(0x1, 0x4)
# define DEV_DISK_OTHER        DEVCLASS(0x1, 0x80)
# define DEV_DISK_RAM          DEVCLASS(0x1, 0x81)

# define DEV_NET_ENET          DEVCLASS(0x2, 0x0)
# define DEV_NET_TOKRING       DEVCLASS(0x2, 0x1)
# define DEV_NET_FDDI          DEVCLASS(0x2, 0x2)
# define DEV_NET_ATM           DEVCLASS(0x2, 0x3)
# define DEV_NET_OTHER         DEVCLASS(0x2, 0x80)

# define DEV_DISPLAY_VGA       DEVCLASS(0x3, 0x0); /* includes 8514 */
# define DEV_DISPLAY_XGA       DEVCLASS(0x3, 0x1);
# define DEV_DISPLAY_OTHER     DEVCLASS(0x3, 0x80);

# define DEV_MULTIMEDIA_VIDEO  DEVCLASS(0x4, 0x0)
# define DEV_MULTIMEDIA_AUDIO  DEVCLASS(0x4, 0x1)
# define DEV_MULTIMEDIA_OTHER  DEVCLASS(0x4, 0x80)

# define DEV_MEM_RAM           DEVCLASS(0x5, 0x0)
# define DEV_MEM_FLASH         DEVCLASS(0x5, 0x1)
# define DEV_MEM_OTHER         DEVCLASS(0x5, 0x80)

/* The following types are probably applicable only to PCI, though
   might get used for VESA interface chips on EISA. */
# define DEV_BRIDGE_HOST       DEVCLASS(0x6, 0x0)
# define DEV_BRIDGE_ISA        DEVCLASS(0x6, 0x1)
# define DEV_BRIDGE_EISA       DEVCLASS(0x6, 0x2)
# define DEV_BRIDGE_MCA        DEVCLASS(0x6, 0x3)
# define DEV_BRIDGE_PCI        DEVCLASS(0x6, 0x4)
# define DEV_BRIDGE_PCMCIA     DEVCLASS(0x6, 0x5)
# define DEV_BRIDGE_NUBUS      DEVCLASS(0x6, 0x6)
# define DEV_BRIDGE_CARDBUS    DEVCLASS(0x6, 0x7)
/* Need definitions for VME, SBUS, .... */
# define DEV_BRIDGE_OTHER      DEVCLASS(0x6, 0x80)

# define DEV_COMM_SERIAL       DEVCLASS(0x7, 0x0)
# define DEV_COMM_PARALLEL     DEVCLASS(0x7, 0x1)
# define DEV_COMM_OTHER        DEVCLASS(0x7, 0x2)

# define DEV_MOTHERBOARD_PIC   DEVCLASS(0x8, 0x0)
# define DEV_MOTHERBOARD_DMA   DEVCLASS(0x8, 0x1)
# define DEV_MOTHERBOARD_TIMER DEVCLASS(0x8, 0x2)
# define DEV_MOTHERBOARD_RTC   DEVCLASS(0x8, 0x3)
# define DEV_MOTHERBOARD_OTHER DEVCLASS(0x8, 0x80)

# define DEV_INPUT_KBD         DEVCLASS(0x9, 0x0)
# define DEV_INPUT_PEN         DEVCLASS(0x9, 0x1) /* digitizer */
# define DEV_INPUT_MOUSE       DEVCLASS(0x9, 0x2)
# define DEV_INPUT_OTHER       DEVCLASS(0x9, 0x80)

# define DEV_DOCK_GENERIC      DEVCLASS(0xA, 0x0)
# define DEV_DOCK_OTHER        DEVCLASS(0xA, 0x80)

# define DEV_CPU_386           DEVCLASS(0xB, 0x0)
# define DEV_CPU_486           DEVCLASS(0xB, 0x1)
# define DEV_CPU_PENTIUM       DEVCLASS(0xB, 0x2)
/* # define DEV_CPU_PENTIUMPRO    DEVCLASS(0xB, 0x3) */
# define DEV_CPU_ALPHA         DEVCLASS(0xB, 0x10)
# define DEV_CPU_PPC           DEVCLASS(0xB, 0x20)
# define DEV_CPU_COPROC        DEVCLASS(0xB, 0x40)

# define DEV_SERBUS_FIREWIRE   DEVCLASS(0xC, 0x0)
# define DEV_SERBUS_ACCESS     DEVCLASS(0xC, 0x1)
# define DEV_SERBUS_SSA        DEVCLASS(0xC, 0x2)
# define DEV_SERBUS_USB        DEVCLASS(0xC, 0x3)
# define DEV_SERBUS_FIBCHAN    DEVCLASS(0xC, 0x4) /* fibre channel */

#endif /* __DEVICEDEFS_H__ */
