#ifndef __PIPEKEY_H__
#define __PIPEKEY_H__

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

/* It actually matters what happens in the hardware data cache -- the
   following value lets an inline transfer occur in the L1 cache on
   all machines we currently support. */
#define PIPE_BUF_SZ 4096


#define OC_Pipe_Read			    1
#define OC_Pipe_Write			    2
#define OC_Pipe_Close			    3

#define RC_EOF                              1

#ifndef __ASSEMBLER__

uint32_t pipe_create(uint32_t krPipeCre, uint32_t krBank,
			     uint32_t krSched, 
			     uint32_t krWpipe /* OUT */,
			     uint32_t krRpipe /* OUT */);

uint32_t pipe_close(uint32_t krPipe);

uint32_t pipe_write(uint32_t krPipe, uint32_t len, const uint8_t *inBuf,
		    uint32_t *outLen);
uint32_t pipe_read(uint32_t krPipe, uint32_t len, uint8_t *outBuf,
		   uint32_t *outLen);
#endif


#endif /* __PIPEKEY_H__ */

