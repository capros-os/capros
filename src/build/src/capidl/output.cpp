/*
 * Copyright (C) 2002, The EROS Group, LLC.
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

#include <string.h>
#include <stdbool.h>

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/Intern.h>
#include <applib/Diag.h>
#include "SymTab.h"
#include "backend.h"

extern void output_symdump(Symbol *);
extern void output_xmldoc(Symbol *);
extern void output_c_hdr(Symbol *);
extern void output_c_hdr_depend(Symbol *);
extern void output_c_stubs(Symbol *);
extern void output_c_stub_depend(Symbol *);
extern void output_capidl(Symbol *);
extern void output_depend(Symbol *);
extern void output_c_server(Symbol *, BackEndFn);
extern void output_c_server_hdr(Symbol *, BackEndFn);

extern void rewrite_for_c(Symbol *);
extern bool c_typecheck(Symbol *);

backend back_ends[] = {
  { "raw",       0, 	      0,             output_symdump,       0  },
  { "xml",       0,           0,             output_xmldoc,        0  },
  { "c-header",  c_typecheck, rewrite_for_c, output_c_hdr,         0  },
  { "c-header-depend", 
                 0,           0,             output_c_hdr_depend,  0  },
  { "c-stubs",   c_typecheck, rewrite_for_c, output_c_stubs,       0  },
  { "c-stub-depend", 
                 0,           0,             output_c_stub_depend, 0  },
  { "c-server",  c_typecheck, rewrite_for_c, 0,       output_c_server  },
  { "c-server-header",
                 c_typecheck, rewrite_for_c, 0,       output_c_server_hdr },
  { "capidl",    0,           0,             output_capidl,        0  },
  { "depend",    0,           0,             output_depend,        0  }
};

backend*
resolve_backend(const char *nm)
{
  size_t i;

  if (nm == 0)
    return &back_ends[0];

  for (i = 0; i < sizeof(back_ends)/ sizeof(backend); i++) {
    if (strcmp(nm, back_ends[i].name) == 0) {
      return &back_ends[i];
    }
  }

  return 0;
}

