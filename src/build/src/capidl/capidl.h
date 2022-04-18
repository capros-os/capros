/*
 * Copyright (C) 2002, The EROS Group, LLC.
 * Copyright (C) 2022, Charles Landau.
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

#include <applib/PtrVec.h>

struct TopsymMap {
  InternedString symName;
  InternedString fileName;
  bool isCmdLine;   /* is this a command-line UOC, as
           opposed to something on the include path? */

  // bool isUOC = true;     /* is this symbol a unit of compilation */
       /* As a FUTURE optimization, we will use the isUOC field in the
       TopsymMap to perform lazy file prescanning. */
};

/*
Create a TopsymMap and add it to uocMap.
*/
TopsymMap *topsym_create(InternedString s, InternedString f, bool isCmdLine);
