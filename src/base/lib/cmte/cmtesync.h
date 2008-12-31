#ifndef __CMTESYNC_H
#define __CMTESYNC_H
/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

/* Common declarations for the CapROS Multi-Threading Environment. */

#include <eros/target.h>	// get result_t
#include <eros/Link.h>
#include <domain/cmte.h>

//typedef uint32_t uva_t;	/* user (unmodified) virtual address */

unsigned int lk_getCurrentThreadNum(void);

typedef struct CMTEWaitQueue {
  Link link;
  unsigned int threadNum;
} CMTEWaitQueue;

void CMTEThread_destroyAll(void);
void * lsync_main(void *);
#define LSYNC_STACK_SIZE 4096

#endif // __CMTESYNC_H
