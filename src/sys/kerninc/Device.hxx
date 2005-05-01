#ifndef __DEVICE_HXX__
#define __DEVICE_HXX__
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

/* EROS device configuration data structure.  After some waffling back
 * and forth, I've decided to adopt the PCI notion of classes and
 * subclasses.  Under this model, all (e.g.) ethernet interfaces will
 * register themselves as CLASS=NETWORK SUBCLASS=Ethernet.  There will
 * be a single 'device creator' key for each class.
 * 
 * Implicit in adopting this design is a change of plan w.r.t. device
 * specific interfaces.
 */


struct Device : public DeviceInfo {
  void (*Invoke)(Invocation& inv);

  static void RegisterDevice(DevInfo&, void (*devFn)(Invocation*));
};

#endif /* __DEVICE_HXX__ */
