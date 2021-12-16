#ifndef __INVOKE_ARM_H__
#define ___INVOKE_ARM_H__
/*
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

/* Architecture-specific declarations for key invocation. */

/* Changes to the Message structure must be reflected in the assembler
 * stubs in sys/arch/.../capstubs/....S,
   base/lib/domain/crt/.../small_rt_hook.S, and other assembler files. */

#include <eros/target.h>	/* get fixreg_t */

#define RESUME_SLOT 3

typedef struct Message {
  uint8_t snd_invKey;		  /* key to be invoked */
  uint8_t invType;
  uint8_t unused0;
  uint8_t unused1;
  
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

  fixreg_t snd_len;

  fixreg_t snd_code;		  /* called this for compatibility */
  fixreg_t snd_w1;
  fixreg_t snd_w2;
  fixreg_t snd_w3;

  const void *snd_data;
  
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

  fixreg_t rcv_limit;
  void *rcv_data;

  fixreg_t rcv_code;			  /* called this for compatibility */
  fixreg_t rcv_w1;
  fixreg_t rcv_w2;
  fixreg_t rcv_w3;

  fixreg_t rcv_sent;
  fixreg_t rcv_keyInfo;
} Message;

#endif /* __INVOKE_ARM_H__ */
