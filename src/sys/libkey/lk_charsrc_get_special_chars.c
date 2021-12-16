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
charsrc_get_special_chars(uint32_t krCharSrc,
			  uint16_t *chars, uint32_t maxNum, uint32_t *actualNum)
{
  uint32_t retval;
  retval = charsrc_control(krCharSrc, CharSrc_Control_GetSpecialChars,
			   0u, 0u, NULL, 0,
			   NULL, NULL,
			   chars, maxNum*sizeof(chars[0]),
			   actualNum);

  if (retval == RC_OK
      && actualNum)
    *actualNum /= sizeof(chars[1]);

  return retval;
}

