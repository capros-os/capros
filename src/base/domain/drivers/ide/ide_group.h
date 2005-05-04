#ifndef __IDE_GROUP_H__
#define __IDE_GROUP_H__
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

/* This file requires #include <kerninc/IoRequest.hxx> */

#define HWIFS_PER_GROUP 2

struct ide_hwif;

struct ide_group {
    struct ide_hwif *hwif[2];		/* needed to disable all interrupts */
    uint8_t cur_hwif;		/* which hwif to do next request on */
  
    struct Request       *curReq;	/* request currently being processed */
} ;


void
init_ide_group( struct ide_group *g);

void
group_StartIO( struct ide_group *group );

#endif /* __IDE_GROUP_H__ */
