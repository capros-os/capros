/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution,
 * and is derived from the EROS Operating System distribution.
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

/* A simple shell domain that asks for a line of input, parses it, and
   executes a small set of commands. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>

#include <stdlib.h>
#include <ctype.h>

#include <eros/cap-instr.h>

#include <domain/ConstructorKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <idl/capros/Process.h>
#include <idl/capros/Stream.h>
#include <idl/capros/eterm.h>

#include <graphics/color.h>
#include <addrspace/addrspace.h>

#include "constituents.h"

#define KR_OSTREAM       KR_APP(0) /* For debugging output via kprintf*/
#define KR_ETERM         KR_APP(1)
#define KR_STRM          KR_APP(2)
#define KR_DEMOFILE      KR_APP(3)
#define KR_NODE          KR_APP(4)
#define KR_TMP           KR_APP(5)

#define DEFAULT_COLOR (0x00F5DEB3)

uint8_t *file = (uint8_t *)0x80000000;

static void
return_to_vger()
{
  Message m;

  memset(&m, 0, sizeof(Message));
  m.rcv_rsmkey = KR_RETURN;
  m.snd_invKey = KR_RETURN;

  SEND(&m);
}

static void
put_string(cap_t kr_eterm, const uint8_t *s)
{
  uint32_t len = strlen(s);
  uint32_t u;

  if (s == NULL)
    return;

  for (u = 0; u < len; u++)
    capros_Stream_write(kr_eterm, s[u]);
}

/* Here's an example of using ANSI escape sequences to get different
   colors, blinking, highlighting, etc. */
static void
menu(cap_t strm)
{
  const char *str = "\033[31mW\033[32me\033[34ml\033[35mc\033[36mo\033[37mm"
    "\033[0me to EROS v.0.0.1pre-alpha.\033[0m\n\r";
  const char *fnt = "Press <F1> to \033[4mchange font\033[0m...\n\r";
  const char *clr = "Press \033[30;47mC\033[0m to change color...\n\r";
  const char *scr = "Press 'S' to test scroll...\n\r";
  const char *qt = "Press 'Q' to quit...\n\r";
  const char *blinking = "\033[5m*** This whole line should be blinking!***\033[0m\n\r";

  put_string(strm, "\033[2J"); /* clear screen */

  put_string(strm, str);
  put_string(strm, fnt);
  put_string(strm, clr);
  put_string(strm, scr);
  put_string(strm, qt);
  put_string(strm, blinking);
  put_string(strm, "\n\r\n\r");
}

int
main(void)
{
  /* Get needed keys from our constituents node */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_DEMOFILE, KR_DEMOFILE);

  /* Get ETerm key
     */
  COPY_KEYREG(KR_ARG(0), KR_ETERM);

  /* Answer our creator, just so it won't block forever. */
  return_to_vger();

  kprintf(KR_OSTREAM, "Test domain says hi ...\n");

  /* Map the contents of the demo file to address 0x80000000 */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_TMP, KR_NODE);
  capros_Process_getAddrSpace(KR_SELF, KR_TMP);
  node_swap(KR_TMP, 16, KR_DEMOFILE, KR_VOID);

  eros_domain_eterm_stream_open(KR_ETERM, KR_STRM);
  eros_domain_eterm_set_cursor(KR_ETERM, 219);
  {
    uint8_t c = '\0';

    color_t color = DEFAULT_COLOR;

    menu(KR_STRM);

    while(toupper(c) != 'Q') {
      capros_Stream_read(KR_STRM, &c);
      if (toupper(c) == 'C') {
	color = (color == DEFAULT_COLOR) ? YELLOW : DEFAULT_COLOR;
	eros_domain_eterm_set_bg_color(KR_ETERM, color);
      }
      else if (toupper(c) == 'S') {
	uint32_t u = 0;

	for (u = 0; u < 588; u++) {
	  capros_Stream_write(KR_STRM, file[u]);
	  if (file[u] == '\n')
	    capros_Stream_write(KR_STRM, '\r');
	}

	capros_Stream_read(KR_STRM, &c);
	menu(KR_STRM);
      }
    }
  }

  eros_domain_eterm_sepuku(KR_ETERM);
  kprintf(KR_OSTREAM, "Test is done.\n");

  return 0;
}

