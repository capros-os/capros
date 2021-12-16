#ifndef __VMWARE_IO_H__
#define __VMWARE_IO_H__

/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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

#define SUPPORTED_VMWARE_ID 2

/* declared in vmware_svga.c */
extern uint32_t card_version_id;
extern uint32_t base_address_reg;

/* For writing/reading to SVGA registers: */
#define VMWRITE(r,x) vmwWrite(card_version_id, base_address_reg, r, x)
#define VMREAD(r) vmwRead(card_version_id, base_address_reg, r)

void vmwWrite(uint32_t card_id, uint32_t port, uint32_t index, uint32_t value);
uint32_t vmwRead(uint32_t card_id, uint32_t port, uint32_t index);

#endif
