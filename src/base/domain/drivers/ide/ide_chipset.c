/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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

//#include <kerninc/kernel.hxx>
//#include <kerninc/MsgLog.hxx>
//#include <kerninc/PCI.hxx>
//#include <kerninc/PCI-def.hxx>
#include <eros/target.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/machine/io.h>
#include <eros/DevicePrivs.h>
#include <domain/ConstructorKey.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include "ide_ide.hxx"

/* functional units on the following list are known bad, and for the
 * time being we don't use them:
 */
static struct BlackList {
  uint16_t vendor;
  uint16_t device;
  char *name;
} IdeBlackList[] = {
    //  { PCI_VENDOR_ID_CMD, PCI_DEVICE_ID_CMD_640, "CMD PCI0640B" },
    //  { PCI_VENDOR_ID_PCTECH, PCI_DEVICE_ID_PCTECH_RZ1000, "PC-TECH RZ1000" },
    //  { PCI_VENDOR_ID_PCTECH, PCI_DEVICE_ID_PCTECH_RZ1001, "PC-TECH RZ1001" }
};

const int nBlackguards = sizeof(IdeBlackList) / sizeof(BlackList);

void
IDE::InitChipsets()
{
#if 0
    if (PciBios::Present()) {

    for (int i = 0; i < nBlackguards; i++) {
      uint8_t bus;
      uint8_t fn;

      if (PciBios::FindDevice(IdeBlackList[i].vendor,
			      IdeBlackList[i].device,
			      0, bus, fn) != PciBios::DeviceNotFound) {
	MsgLog::printf(
"FATAL: Your machine contains a PCI-IDE controller chip that has very\n"
"       serious flaws: the %s.\n", IdeBlackList[i].name);

	if (bus == 0) MsgLog::printf(
"\n"
"       The offending chip is soldered into your motherboard.\n");
	MsgLog::printf(
"\n"
"       Since these flaws can lead to corruption of data, and we have\n"
"       not had an opportunity to test our workarounds adequately, EROS\n"
"       currently does not run on machines containing these chips.\n"
"\n"
"       Windows 95 and Windows NT 3.5 or later include workarounds for\n"
"       these flaws.  If you are running earlier versions, it's past time\n"
"       to upgrade either your OS or your PCI board.\n"
"\n"
"       We apologize for any inconvenience this may have caused you.\n");
	halt('a');
      }
    }
  }
#endif
}
