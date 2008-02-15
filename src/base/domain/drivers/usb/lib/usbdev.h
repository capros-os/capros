#ifndef _LIB_USBDEV_H_
#define _LIB_USBDEV_H_

/*
 * Copyright (C) 2008, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
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

#define KR_USBINTF KR_APP2(0)

int usbdev_buffer_create(void);
void usbdev_buffer_destroy(void);
void usb_unlink_endpoint(unsigned long endpoint);
void usb_kill_endpoint(unsigned long endpoint);
void usb_clear_rejecting(unsigned long endpoint);
int usb_submit_urb_wait(struct urb * urb, gfp_t mem_flags);

void usb_settoggle(struct usb_device * dev, unsigned char endpointNum,
  unsigned int out, unsigned int bit);

#endif // _LIB_USBDEV_H_
