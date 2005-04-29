#ifndef PARSETYPE_H
#define PARSETYPE_H

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

/*
 * Token type structure. Using a structure for this is a quite
 * amazingly bad idea, but using a union makes the C++ constructor
 * logic unhappy....
 */

typedef struct Token Token;
struct Token {
  /* Context at which token was found */
  InternedString file;
  unsigned line;

  /* Character representation of token: */
  InternedString is;
};

typedef struct ParseType ParseType;
struct ParseType {
  Token     tok;		/* a literal string from the
				 * tokenizer. Used for strings,
				 * characters, numerical values. */

  Symbol    *sym;

  unsigned  flags;		/* code generation flags. See SymTab.h */
};

#endif /* PARSETYPE_H */
