#ifndef __SIGMUXKEY_H__
#define __SIGMUXKEY_H__

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

#define OC_SigMux_Post			    1
#define OC_SigMux_Wait			    2
#define OC_SigMux_MakeRecipient		    128

#ifndef __ASSEMBLER__

uint32_t sigmux_post(uint32_t krSigMux, uint32_t sigs);
uint32_t sigmux_wait(uint32_t krSigMux, uint32_t sigs, uint32_t *wakeSigs);
uint32_t sigmux_make_recipient(uint32_t krSigMux, uint32_t ndx,
			       /* OUT */ uint32_t krRecipient);

#endif


#endif /* __SIGMUXKEY_H__ */

