#ifndef __DEVICEKEY_H__
#define __DEVICEKEY_H__

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

/*
 * This file resides in eros/ because the kernel and the invocation
 * library various must agree on the values.
 */

/* Local Variables: */
/* comment-column:34 */
/* End: */

/* ORDER and RESULT code values: */

#define OC_Device_Mount		1
#define OC_Device_Unmount	2

#define OC_Device_Read		16
#define OC_Device_Write		17
#define OC_Device_Rewind	18

#define RC_NoSuchDevice		1
#define RC_NotMounted		2
#define RC_NotPageAddr		3
#define RC_TooBig		4

#ifndef __ASSEMBLER__
uint32_t device_read(uint32_t krDevice, uint32_t startSec, uint32_t nSec, uint8_t *buf);
uint32_t device_write(uint32_t krDevice, uint32_t startSec, uint32_t nSec, uint8_t *buf);
uint32_t device_mount(uint32_t krDevice);
uint32_t device_unmount(uint32_t krDevice);
#endif

#endif /* __DEVICEKEY_H__ */
