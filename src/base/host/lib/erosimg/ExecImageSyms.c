/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

/* binutils (correctly) does not ship its config.h, but it attempts to check
 * in its public API that config.h has been included already by checking
 * the following variables. */

#define PACKAGE 1
#define PACKAGE_VERSION 1

#include <bfd.h>
#include <string.h>

#include <erosimg/App.h>
#include <erosimg/Intern.h>
#include <erosimg/ExecImage.h>

/* Somewhere along the way, the binutils people changed bfd.h in an
   incompatible way. Depending on what version you have, this needs to
   be case to either (bfd_boolean)0 or (enum bfd_boolean)0. Rather
   than fight city hall, I've just given up on the casting
   altogether. */
#define BFD_FALSE 0

extern char* target;

bool
xi_GetSymbolValue(ExecImage *pImage, const char *symName, uint32_t *pValue)
{
  bfd *bfd_file;
  long symcount;
  PTR minisyms;
  unsigned int size;
  bfd_byte *from, *fromend;
  char **matching;
  const char *imageFileName;
  asymbol *store;
  
  bfd_init ();

  imageFileName = app_BuildPath(pImage->name);

#if 0
  diag_printf("Fetching symbols from \"%s\"\n", imageFileName);
#endif

  bfd_file = bfd_openr(imageFileName, "default");

#if 0
  diag_printf("bfd * ix 0x%08x\n", bfd_file);
#endif

  if (bfd_check_format_matches (bfd_file, bfd_object, &matching) == 0) {
    diag_printf("\"%s\" is not an executable\n", bfd_get_filename
		 (bfd_file));
    return false;
  }

  if (!(bfd_get_file_flags (bfd_file) & HAS_SYMS)) {
    diag_printf("No symbols in \"%s\".\n", bfd_get_filename
		 (bfd_file));
    return false;
  }

  symcount = bfd_read_minisymbols (bfd_file, BFD_FALSE, &minisyms, &size);
  if (symcount < 0) {
    diag_printf("File \"%s\" had no symbols.\n", pImage->name);
    return false;
  }

  from = (bfd_byte *) minisyms;
  fromend = from + symcount * size;

  store = bfd_make_empty_symbol (bfd_file);

  for (; from < fromend; from += size) {
    asymbol *sym;

    sym = bfd_minisymbol_to_symbol (bfd_file, BFD_FALSE, from, store);
    if (sym == NULL)
      diag_fatal(3, "Could not fetch symbol info!\n");

    if (strcmp(bfd_asymbol_name (sym), symName) == 0) {
      symbol_info syminfo;

      bfd_get_symbol_info (bfd_file, sym, &syminfo);

      *pValue = syminfo.value;
      return true;
    }
  }

  return false;
}

