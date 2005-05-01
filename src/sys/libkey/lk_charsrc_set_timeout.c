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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/CharSrcProto.h>

/* Wait for a tty event */
uint32_t
charsrc_set_timeout(uint32_t krCharSrc,
		    uint32_t initChars, uint32_t timeoutMs)
{
  return charsrc_control(krCharSrc, CharSrc_Control_SetTimeout,
			 initChars, timeoutMs,
			 NULL, 0,
			 NULL, NULL,
			 NULL, 0u, NULL);
}

