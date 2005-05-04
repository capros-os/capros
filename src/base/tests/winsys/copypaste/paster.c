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

#define KR_OSTREAM         KR_APP(0) /* For debugging output via kprintf*/
#define KR_ETERM           KR_APP(1)
#define KR_STRM            KR_APP(2)
#define KR_NODE            KR_APP(3)
#define KR_TMP             KR_APP(4)
#define KR_PASTECONTENT    KR_APP(5)
#define KR_PASTECONVERTER  KR_APP(6)

#define DEFAULT_COLOR (0x00F5DEB3)

#define PASTE_INDICATOR  0x0016

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

static void
menu(cap_t strm)
{
  const char *str = "Press \033[30;47mCTRL-V\033[0m to paste text in this window...\n\r";

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
    "Paster"
  };

  /* Get needed keys from our constituents node */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  /* Get ETerm key
     */
  COPY_KEYREG(KR_ARG(0), KR_ETERM);

  /* Answer our creator, just so it won't block forever. */
  return_to_vger();

  kprintf(KR_OSTREAM, "Paster domain says hi ...\n");

  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_TMP, KR_NODE);

  eros_domain_eterm_stream_open(KR_ETERM, KR_STRM);
  eros_domain_eterm_set_cursor(KR_ETERM, 219);
  eros_domain_eterm_set_title(KR_ETERM, Title);

  {
    uint8_t c = '\0';

    kprintf(KR_OSTREAM, "PASTER calling menu()\n");

    menu(KR_STRM);

    while(toupper(c) != 'Q') {
      kprintf(KR_OSTREAM, "PASTER calling stream_read().\n");
      eros_Stream_read(KR_STRM, &c);
      if (c == PASTE_INDICATOR) {
	uint32_t u = 0;

	kprintf(KR_OSTREAM, "PASTER executing GetPasteBuffer...\n");

	if (eros_domain_eterm_get_pastebuffer(KR_ETERM, KR_PASTECONTENT, 
					      KR_PASTECONVERTER)
	    != RC_OK)
	  kprintf(KR_OSTREAM, "Uh-oh! Paste couldn't get the pastebuffer!\n");
	else {

	  /* Map the contents of the pastebuffer contents to address
	     0x80000000 */
	  process_copy(KR_SELF, ProcAddrSpace, KR_TMP);
	  node_swap(KR_TMP, 16, KR_PASTECONTENT, KR_VOID);

	  for (u = 0; u < 220; u++) {
	    eros_Stream_write(KR_STRM, file[u]);
	    if (file[u] == '\n')
	      eros_Stream_write(KR_STRM, '\r');
	  }
	}
	eros_Stream_read(KR_STRM, &c);
	menu(KR_STRM);
      }
    }
  }

  eros_domain_eterm_sepuku(KR_ETERM);
  kprintf(KR_OSTREAM, "Paster is done.\n");

  return 0;
}

