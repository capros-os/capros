/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
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

/* A simple shell domain that asks for a line of input, parses it, and
   executes a small set of commands. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>

#include <stdlib.h>
#include <ctype.h>

#include <eros/cap-instr.h>

#include <domain/ConstructorKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <idl/eros/Stream.h>
#include <idl/eros/domain/eterm.h>

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

#define COPY_INDICATOR  0x0003

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
    eros_Stream_write(kr_eterm, s[u]);
}

/* Here's an example of using ANSI escape sequences to get different
   colors, blinking, highlighting, etc. */
static void
menu(cap_t strm)
{
  const char *str = "Press \033[30;47mCTRL-C\033[0m in this window to place "
    "text in the paste buffer\n\r";

  put_string(strm, "\033[2J"); /* clear screen */

  put_string(strm, str);
  put_string(strm, "\n\r\n\r");
}

int
main(void)
{
  eros_domain_eterm_titlestr Title = {
    7,
    7,
    "Copier"
  };

  /* Get needed keys from our constituents node */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_DEMOFILE, KR_DEMOFILE);

  /* Get ETerm key
     */
  COPY_KEYREG(KR_ARG(0), KR_ETERM);

  /* Answer our creator, just so it won't block forever. */
  return_to_vger();

  kprintf(KR_OSTREAM, "Copier domain says hi ...\n");

  /* Map the contents of the demo file to address 0x80000000 */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_TMP, KR_NODE);
  process_copy(KR_SELF, ProcAddrSpace, KR_TMP);
  node_swap(KR_TMP, 16, KR_DEMOFILE, KR_VOID);

  eros_domain_eterm_stream_open(KR_ETERM, KR_STRM);
  eros_domain_eterm_set_cursor(KR_ETERM, 219);
  eros_domain_eterm_set_title(KR_ETERM, Title);

  {
    uint8_t c = '\0';

    menu(KR_STRM);

    while(toupper(c) != 'Q') {
      eros_Stream_read(KR_STRM, &c);
      if (c == COPY_INDICATOR) {
	if (eros_domain_eterm_put_pastebuffer(KR_ETERM, KR_DEMOFILE, 
					      KR_DEMOFILE) == RC_OK)
	  kprintf(KR_OSTREAM, "Copier just executed PutPasteBuffer...\n");
	else
	  kprintf(KR_OSTREAM, "Copier failed in call to PutPasteBuffer!\n");
      }
    }
  }

  eros_domain_eterm_sepuku(KR_ETERM);
  kprintf(KR_OSTREAM, "Copier is done.\n");

  return 0;
}

