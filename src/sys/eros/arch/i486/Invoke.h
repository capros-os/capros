#ifndef __INVOKE_I486_H__
#define ___INVOKE_I486_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Architecture-specific declarations for key invocation. */

/* Changes to the Message structure must be reflected in the assembler
 * stubs in sys/arch/.../capstubs/....S and other assembler files. */

#include <eros/target.h>	/* get fixreg_t */

#define RESUME_SLOT 3

typedef struct Message {
  fixreg_t invType;
  fixreg_t snd_invKey;		  /* key to be invoked */

  fixreg_t snd_len;
  const void *snd_data;
  
#if RESUME_SLOT == 0
  uint8_t snd_rsmkey;
  uint8_t snd_key0;
  uint8_t snd_key1;
  uint8_t snd_key2;
#else
  uint8_t snd_key0;
  uint8_t snd_key1;
  uint8_t snd_key2;
  uint8_t snd_rsmkey;
#endif

  fixreg_t rcv_limit;
  void *rcv_data;
  
#if RESUME_SLOT == 0
  uint8_t rcv_rsmkey;
  uint8_t rcv_key0;
  uint8_t rcv_key1;
  uint8_t rcv_key2;
#else
  uint8_t rcv_key0;
  uint8_t rcv_key1;
  uint8_t rcv_key2;
  uint8_t rcv_rsmkey;
#endif

  fixreg_t snd_code;		  /* called this for compatibility */
  fixreg_t snd_w1;
  fixreg_t snd_w2;
  fixreg_t snd_w3;


  fixreg_t rcv_code;			  /* called this for compatibility */
  fixreg_t rcv_w1;
  fixreg_t rcv_w2;
  fixreg_t rcv_w3;

  uint16_t rcv_keyInfo;
  fixreg_t rcv_sent;
} Message;

#endif /* __INVOKE_I486_H__ */
