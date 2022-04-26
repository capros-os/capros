/*
 * Copyright (C) 2002, The EROS Group, LLC.
 * Copyright (C) 2008, 2009, 2011, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <assert.h>
#include <o_c_util.h>
#include "util.h"

/* Size of largest integral type that we will attempt to registerize: */
#define MAX_REGISTERIZABLE 64


/* NOTE: the 'indent' argument is NOT used here in the normal case. It
   is provided to indicate the indentation level at which the type is
   being output, so that types which require multiline output can be
   properly indented. The ONLY case in which do_indent() should be
   called from this routine is the multiline type output case. */
void
output_c_type(Symbol *s, FILE *out, int indent)
{
  s = symbol_ResolveRef(s);

  /* If this symbol is a typedef, we want to put out the typedef name
     as its type name. Since the subsequent type classification checks
     will bypass typedefs to expose the underlying type, handle
     typedefs here. */

  if (s->cls == sc_typedef) {
    fprintf(out, "%s", symbol_QualifiedName(s,'_'));
    return;
  }

  s = symbol_ResolveType(s);

  if (symbol_IsVarSequenceType(s)) {
    s = s->type;
    fprintf(out, "struct {\n");
    do_indent(out, indent+2);
    fprintf(out, "unsigned long max;\n");
    do_indent(out, indent+2);
    fprintf(out, "unsigned long len;\n");
    do_indent(out, indent+2);
    output_c_type(s, out, indent);
    fprintf(out, " *data;\n");
    do_indent(out, indent);
    fprintf(out, "}");

    return;
  }

  if (symbol_IsFixSequenceType(s))
    s = s->type;

  while (s->cls == sc_symRef)
    s = s->value;

  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty){
      case lt_unsigned:
    switch(mpz_get_ui(s->v.i)) {
    case 0:
      fprintf(out, "Integer");
      break;
    case 8:
      fprintf(out, "uint8_t");
      break;
    case 16:
      fprintf(out, "uint16_t");
      break;
    case 32:
      fprintf(out, "uint32_t");
      break;
    case 64:
      fprintf(out, "uint64_t");
      break;
    default:
      fprintf(out, "/* unknown integer size */");
      break;
    };

    break;
      case lt_integer:
    switch(mpz_get_ui(s->v.i)) {
    case 0:
      fprintf(out, "Integer");
      break;
    case 8:
      fprintf(out, "int8_t");
      break;
    case 16:
      fprintf(out, "int16_t");
      break;
    case 32:
      fprintf(out, "int32_t");
      break;
    case 64:
      fprintf(out, "int64_t");
      break;
    default:
      fprintf(out, "/* unknown integer size */");
      break;
    };

    break;
      case lt_char:
    switch(mpz_get_ui(s->v.i)) {
    case 0:
    case 32:
      fprintf(out, "wchar_t");
      break;
    case 8:
      fprintf(out, "char");
      break;
    default:
      fprintf(out, "/* unknown wchar size */");
      break;
    }
    break;
      case lt_string:
    switch(mpz_get_ui(s->v.i)) {
    case 0:
    case 32:
      fprintf(out, "wchar_t *");
      break;
    case 8:
      fprintf(out, "char *");
      break;
    default:
      fprintf(out, "/* unknown string size */");
      break;
    }
    break;
      case lt_float:
    switch(mpz_get_ui(s->v.i)) {
    case 32:
      fprintf(out, "float");
      break;
    case 64:
      fprintf(out, "double");
      break;
    case 128:
      fprintf(out, "long double");
      break;
    default:
      fprintf(out, "/* unknown float size */");
      break;
    }
    break;
      case lt_bool:
    {
      fprintf(out, "bool");
      break;
    }
      case lt_void:
    {
      fprintf(out, "void");
      break;
    }
      default:
    fprintf(out, "/* Bad primtype type code */");
    break;
      }
      break;
    }
  case sc_symRef:
  case sc_typedef:
    {
      /* Should have been caught by the symbol resolution calls above */
      assert(false);
      break;
    }
  case sc_interface:
  case sc_absinterface:
    {
      fprintf(out, "cap_t");
      break;
    }
  case sc_struct:
  case sc_union:
    {
      fprintf(out, "%s", symbol_QualifiedName(s,'_'));
      break;
    }

  case sc_seqType:
    {
      fprintf(out, "/* some varSeqType */");
      break;
    }

  case sc_arrayType:
    {
      fprintf(out, "/* some fixSeqType */");
      break;
    }

  default:
    fprintf(out, "%s", symbol_QualifiedName(s,'_'));
    break;
  }
}

/* can_registerize(): given a symbol /s/ denoting an argument, and a
   number /nReg/ indicating how many register slots (data words in the message) are taken, return:

     0   if the symbol /s/ cannot be passed in registers, or
     N>0 the number of registers that the symbol /s/ will occupy.

   note that a 64 bit integer quantity will be registerized, but only
   if it can be squeezed into the number of registers that remain
   available.
*/
unsigned 
can_registerize(Symbol *s, unsigned nReg)
{
  if (nReg == MAX_REGS)
    return 0;
  assert(nReg < MAX_REGS);

  s = symbol_ResolveType(s);
  
  assert(symbol_IsBasicType(s));

  while (s->cls == sc_symRef)
    s = s->value;

  switch(s->cls) {
  case sc_typedef:
    return can_registerize(s->type, nReg);

  case sc_primtype:
    {
      switch(s->v.lty) {
      case lt_integer:
      case lt_unsigned:
      case lt_char:
      case lt_bool:
      case lt_float:
    {
      /* Integral types are aligned to their size. Floating types
         are aligned to their size, not to exceed 64 bits. If we
         ever support larger fixed integers, we'll need to break
         these cases up. */

      unsigned bits = mpz_get_ui(s->v.i);
      
      if (bits == 0)
        return 0;

      if (nReg >= MAX_REGS)
        return 0;

      if (bits <= REGISTER_BITS)
        return 1;

      if (bits <= MAX_REGISTERIZABLE) {
        unsigned needRegs = bits / REGISTER_BITS;
        if (nReg + needRegs <= MAX_REGS)
          return needRegs;
      }

      return 0;
    }
      default:
    return 0;
      }
    }
  case sc_enum:
    {
      /* Note assumption that enumeral type fits in a register */
      return 1;
    }

  default:
    return 0;
 }
}

unsigned
emit_symbol_align(const char *lenVar, FILE *out, int indent,
          unsigned elemAlign, unsigned align)
{
  if ((elemAlign & align) == 0) {
    do_indent(out, indent);
    fprintf(out, "%s += (%d-1); %s -= (%s %% %d);\t/* align to %d */\n",
        lenVar, elemAlign,
        lenVar, lenVar, elemAlign, elemAlign);
  }

  return (elemAlign * 2) - 1;
}

void analyze_arguments(Symbol * s, AnalyzedArgs & analArgs)
{
  unsigned  inNReg = FIRST_REG;  // first available  IN data register
  unsigned outNReg = FIRST_REG;  // first available OUT data register

  for (const auto eachChild : s->children) {
    FormalSym * fsym = dynamic_cast<FormalSym*>(eachChild);
    assert(fsym);
    
    if (symbol_IsInterface(fsym->type)) {   // a key
      if (fsym->isOutput)
        analArgs.outKeyRegs.push_back(fsym);
      else
        analArgs.inKeyRegs.push_back(fsym);
    }
    else {  // It's data.
      unsigned needRegs;
      if (fsym->isOutput) {
        if ((needRegs = can_registerize(fsym->type, outNReg)) != 0) {
          // Data in OUT register(s)
          outNReg += needRegs;
          analArgs.outDataRegs.push_back(fsym);
        }
        else {
          analArgs.outString.push_back(fsym);
        }
      }
      else {    // input
        if ((needRegs = can_registerize(fsym->type, inNReg)) != 0) {
          // Data in IN register(s)
          inNReg += needRegs;
          analArgs.inDataRegs.push_back(fsym);
        }
        else {
          analArgs.inString.push_back(fsym);
        }
      }
    }
  }
}
