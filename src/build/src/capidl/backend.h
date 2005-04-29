#ifndef BACKEND_H
#define BACKEND_H
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

typedef void (*BackEndFn)(Symbol *);
typedef bool (*BackEndCheckFn)(Symbol *);
typedef void (*ScopeWalker)(Symbol *scope, BackEndFn outfn);

typedef struct backend backend;
struct backend {
  const char *name;
  BackEndCheckFn typecheck;
  BackEndFn      xform;
  BackEndFn      fn;
  ScopeWalker    scopefn;
};

backend *resolve_backend(const char *nm);

#endif /* BACKEND_H */
