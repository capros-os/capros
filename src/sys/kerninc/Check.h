#ifndef __CHECK_H__
#define __CHECK_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

/* Check: thin wrapper around the consistency checking routines */

/* Former member functions of Check */

/*typedef struct ObjectHeader ObjectHeader;*/
#include <kerninc/ObjectHeader.h>

void check_DoConsistency( const char * );

void check_Consistency( const char * );

bool check_Pages();

bool check_Nodes();

bool check_Contexts(const char *);

#ifdef USES_MAPPING_PAGES
bool check_MappingPage(PageHeader *);
#endif

#endif /* __CHECK_H__ */
