/*
 * Copyright (C) 2003, The EROS Group, LLC.
 * Copyright (C) 2009, Strawberry Development Group.
 * Copyright (C) 2022, Charles Landau.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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

#include <SymTab.h>
#include <applib/PtrVec.h>

extern PtrVec *extract_registerizable_arguments(Symbol *s, SymClass sc);
extern PtrVec *extract_string_arguments(Symbol *s, SymClass sc);

extern void output_c_type(Symbol *s, FILE *out, int indent);

extern unsigned can_registerize(Symbol *s, unsigned nReg);
extern unsigned emit_symbol_align(const char *lenVar, FILE *out, int indent,
                  unsigned elemAlign, unsigned align);

#define FIRST_REG 1     /* reserve 1 for opcode/result code */

/* Size of native register */
#define REGISTER_BITS      32

#define MAX_REGS  4
