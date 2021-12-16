#ifndef OBJSPACE_H
#define OBJSPACE_H

/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

struct Bank;

extern void ob_init(void);
/* ob_init():
 *     Initializes master frame space.  Called once on startup.  Calls
 *   Bank_init().
 */

uint32_t ob_AllocFrame(struct Bank *, OID* oid, unsigned int baseType);
#define ob_AllocPageFrame(bank, oid) \
  ob_AllocFrame(bank, oid, capros_Range_otPage)
#define ob_AllocNodeFrame(bank, oid) \
  ob_AllocFrame(bank, oid, capros_Range_otNode)

void ob_ReleaseFrame(struct Bank*, OID oid, unsigned int baseType);
#define ob_ReleasePageFrame(bank, oid) \
  ob_ReleaseFrame(bank, oid, capros_Range_otPage)
#define ob_ReleaseNodeFrame(bank, oid) \
  ob_ReleaseFrame(bank, oid, capros_Range_otNode)

#endif /* OBJSPACE_H */
