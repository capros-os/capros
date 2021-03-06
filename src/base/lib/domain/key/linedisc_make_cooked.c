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
#include <domain/LineDiscKey.h>

uint32_t
linedisc_make_cooked(uint32_t krLineDisc,
                     uint16_t *specChars,
                     uint32_t numChars)
{
   return linedisc_set_inp_proc(krLineDisc,LD_In_DefaultCookedFlags)
          || linedisc_set_outp_proc(krLineDisc,LD_Out_DefaultCookedFlags)
          || charsrc_set_special_chars(krLineDisc,specChars,numChars);
   /* returns RC_OK only if all of them return RC_OK */
}
