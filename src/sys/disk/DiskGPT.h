#ifndef __DISK_DISKGPT_H__
#define __DISK_DISKGPT_H__
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

/* For a GPT, the first byte of nodeData contains: */
#define GPT_L2V_MASK 0x3f
#define GPT_BACKGROUND 0x40
#define GPT_KEEPER 0x80

#ifndef __ASSEMBLER__

INLINE uint8_t * 
gpt_l2vField(uint16_t * nodeDatap)
{
  return (uint8_t *) nodeDatap;
}

#endif // __ASSEMBLER__

#endif /* __DISK_DISKGPT_H__ */
