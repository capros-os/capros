#ifndef __NETDEV_HXX__
#define __NETDEV_HXX__
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

struct Invocation;

/* Common base class for all network controllers: */

struct NetDev {
  bool isRegistered;
public:
  char      *name;
  uint16_t  devClass;
  uint32_t      nUnits;

  static NetDev* registry[];
  
  void Register();
#if 0
  void Unregister();
#endif
  
  virtual void Invoke(Invocation& inv) = 0;
  
  NetDev();

  virtual ~NetDev()
  {
#if 0
    Unregister();
#endif
  }
};

#endif /* __NETDEV_HXX__ */