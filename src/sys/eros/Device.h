#ifndef __DEVICE_H__
#define __DEVICE_H__
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

/* I separated the Device definitions so that things would be easier
   to work with. */
#include <eros/DeviceDef.h>

/* The DeviceInfo structure provides a specification of the system
   equipment list, and can be used to determine current resource
   allocation. */

/* NOTE: All fields in this structure follow LITTLE ENDIAN conventions */

struct DeviceInfo {
  char          name[16];	/* short device name */
  uint16_t	vendorID;
  uint16_t	devID;
  uint16_t      devClass;
  uint8_t		devInterface;
  uint8_t		revsion;
  uint8_t          interrupt;	/* 255=none/undefined */
  uint32_t          paddr;		/* physical memory address, 0=none */
  uint32_t          psz;		/* physical memory size */
  uint32_t          ioaddr;		/* io port space start address */
  uint32_t          iosz;		/* io port space size */
};


#endif /* __DEVICE_H__ */
