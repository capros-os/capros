#ifndef __PCI_H__
#define __PCI_H__
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

/* This interface, and the supporting code, is based on:
 * 
 *   PCI BIOS Specification Revision
 *   PCI Local Bus Specification
 *   PCI System Design Guide
 * 
 *   PCI Special Interest Group
 *   M/S HF3-15A
 *   5200 N.E. Elam Young Parkway
 *   Hillsboro, Oregon 97124-6497
 *   +1 (503) 696-2000 
 *   +1 (800) 433-5177
 * 
 */


/* Conceptually, we assume that the PCI BIOS is implemented by all
 * platforms that support PCI.  The actual implementation of the PCI
 * BIOS is machine specific.  Much of the rest of the PCI code relies
 * on this as the basis of its machine independence.
 */

enum PciBiosErrors {
  OK = 0x00,
  FuncNotSupported = 0x81,
  BadVendorId = 0x83,
  DeviceNotFound = 0x86,
  BadRegisterNumber = 0x87,
  SetFailed =	0x88,
  BufferTooSmall = 0x89
} ;




extern bool pciBios_isInit;


/* Former member functions of PciBios */

/* private members */

/* initialize PCI BIOS -- returns new memory end */
void pciBios_Init();

/* public members */

/* return true if PCI BIOS present */
bool pciBios_Present();

/* Do platform-specific fixups: */
void pciBios_Fixup();

/* Find the next instance of a PCI device in a particular class: */
uint32_t pciBios_FindClass(uint32_t devClass, uint16_t ndx, uint8_t *bus /*@ not null @*/, 
                           uint8_t *devfn /*@ not null @*/);

/* Find the next instance of a particular PCI device: */
uint32_t pciBios_FindDevice(uint16_t vendor, uint16_t devId,
                            uint16_t ndx, uint8_t* bus /*@ not null @*/, uint8_t *devfn /*@ not null @*/);

uint32_t pciBios_ReadConfig8(uint8_t bus, uint8_t devfn, uint8_t where, uint8_t* val /*@ not null @*/);
uint32_t pciBios_ReadConfig16(uint8_t bus, uint8_t devfn, uint8_t where,
                              uint16_t* val /*@ not null @*/);
uint32_t pciBios_ReadConfig32(uint8_t bus, uint8_t devfn, uint8_t where,
                              uint32_t* val /*@ not null @*/);

uint32_t pciBios_WriteConfig8(uint8_t bus, uint8_t devfn, uint8_t where, uint8_t val);
uint32_t pciBios_WriteConfig16(uint8_t bus, uint8_t devfn, uint8_t where,
                               uint16_t val);
uint32_t pciBios_WriteConfig32(uint8_t bus, uint8_t devfn, uint8_t where,
                               uint32_t val);

/* Following is machine independent: */
const char *pciBios_StrError(uint32_t error);



#if 0
struct PciDeviceInfo {
  uint16_t   vendor;
  uint16_t   device;

  const char *name;
  uint8_t       bridge_type;	/* bridge type or 0xff */
};

struct PCI {
  static struct PciDevice *devices;

  static bool CanBusMaster(uint8_t bus, uint8_t devfn);
#if 0
  void BurstBridge(uint8_t bus, uint8_t devFn, uint8_t ty);
#endif
  
  static uint8_t ScanBus(struct PciBus*);
  static void Init();

  static PciDeviceInfo *LookupDev(uint16_t vendor, uint16_t devId);
};
#endif

#endif /* __PCI_H__ */
