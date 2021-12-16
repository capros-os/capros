/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


/*
 * eros.pci.h
 *
 * This is the master database for EROS, from which all of the PCI
 * header files are generated.  If you come up with more information,
 * please let us know!
 *
 * The original of this list came from Jim Boemler's web page at
 *    HTTP://www.halcyon.com/, and we
 * periodically update it from their and from the linux distribution.
 *
 * Created automatically from the web using the following URL:
 *     http: *www.halcyon.com/scripts/jboemler/pci/pcicode 
 * Software to create and maintain the PCICODE List written by:
 *     Jim Boemler (jboemler@halcyon.com) 
 * This is header number 358, generated 03-25-96.
 */

typedef struct PciBaseClass {
  int            badEntry;
  unsigned char  BaseClassId ;
  char *	 BaseClassName;
} PciBaseClass;
extern PciBaseClass PciBaseClassTable[];
extern unsigned  num_pci_base_classes;

typedef struct PciClass {
  int            badEntry;
  unsigned short ClassId ;
  char *	 ClassName;
} PciClass;
extern PciClass PciClassTable[];
extern unsigned  num_pci_classes;

typedef struct PciVendor {
  int            badEntry;
  unsigned short VenId ;
  char *	 VenShort;
  char *	 VenFull;
} PciVendor;

extern PciVendor PciVendorTable[];
extern unsigned  num_pci_vendors;

typedef struct PciDevice {
  int            badEntry;
  unsigned short VenId ;
  unsigned short DevId ;
  char           *Chip ;
  char           *ChipDesc ;
}  PciDevice;
extern PciDevice PciDeviceTable[];
extern unsigned num_pci_devices;
