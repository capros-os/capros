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

#include <vector>
#include <SymTab.h>

extern void output_c_type(Symbol *s, FILE *out, int indent);

extern unsigned can_registerize(Symbol *s, unsigned nReg);
extern unsigned emit_symbol_align(const char *lenVar, FILE *out, int indent,
                  unsigned elemAlign, unsigned align);

#define FIRST_REG 1     // First available data register
                        // reserving [0] for snd_code or rcv_code

/* Size of native register */
#define REGISTER_BITS      32

#define MAX_REGS  4     // Total number of data registers

struct StringArg {
  FormalSym * fsym;
  bool direct;      // true iff it is fixed serializable
};


// AnalyzedArgs captures our analysis of the args to an operation/method.
struct AnalyzedArgs {
  std::vector<FormalSym*> inKeyRegs;
  std::vector<FormalSym*> outKeyRegs;
  std::vector<FormalSym*> inDataRegs;
  std::vector<FormalSym*> outDataRegs;
  std::vector<StringArg> inString;
  std::vector<StringArg> outString;
};
void analyze_arguments(Symbol * s, AnalyzedArgs & analArgs);
