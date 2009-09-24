#ifndef __HEAP_H__
#define __HEAP_H__
/*
 * Copyright (C) 2009, Strawberry Development Group.
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

#include <kerninc/Activity.h>
#include <kerninc/Process.h>
#include <kerninc/ObjectHeader.h>
     
#define heap_Size \
  ((  KTUNE_NACTIVITY * sizeof(Activity) \
    + KTUNE_NCONTEXT * sizeof(Process) \
    + KTUNE_MAX_CARDMEM * ((1024*1024)/EROS_PAGE_SIZE) * sizeof(PageHeader) \
    + 4*1024*1024 \
    + EROS_PAGE_SIZE - 1) \
   & ~ EROS_PAGE_MASK)	// round up to page size

#endif /* __HEAP_H__ */
