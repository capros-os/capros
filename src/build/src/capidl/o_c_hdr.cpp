/*
 * Copyright (C) 2002, The EROS Group, LLC.
 * Copyright (C) 2006-2008, 2010, Strawberry Development Group.
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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/xmalloc.h>
#include <applib/Intern.h>
#include <applib/Diag.h>
#include <applib/PtrVec.h>
#include <applib/path.h>
#include "SymTab.h"
#include "util.h"
#include "o_c_util.h"

static void print_asmifdef(FILE *out);
static void print_asmendif(FILE *out);

static void
PrintDocComment(FILE *out, const char *str, int indent)
{
  bool want_fill = false;

  do_indent(out, indent);
  fprintf(out,"/**\n");

  do_indent(out, indent+2);
  
  while (*str) {
    if (want_fill) {
      do_indent(out, indent+2);
      want_fill = false;
    }

    if (*str == '\n')
      want_fill = true;
    fputc(*str++, out);
  }

  do_indent(out, indent);
  fprintf(out, "*/\n");
}

const char* 
c_serializer(Symbol *s)
{
  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty){
      case lt_unsigned:
	switch(mpz_get_ui(s->v.i)) {
	case 0:
	  return "Integer";
	  break;
	case 8:
	  return "u8";
	  break;
	case 16:
	  return "u16";
	  break;
	case 32:
	  return "u32";
	  break;
	case 64:
	  return "u64";
	  break;
	default:
	  return "/* unknown integer size */";
	  break;
	};

	break;
      case lt_integer:
	switch(mpz_get_ui(s->v.i)) {
	case 0:
	  return "Integer";
	  break;
	case 8:
	  return "i8";
	  break;
	case 16:
	  return "i16";
	  break;
	case 32:
	  return "i32";
	  break;
	case 64:
	  return "i64";
	  break;
	default:
	  return "/* unknown integer size */";
	  break;
	};

	break;
      case lt_char:
	switch(mpz_get_ui(s->v.i)) {
	case 0:
	case 32:
	  return "wchar_t";
	  break;
	case 8:
	  return "char";
	  break;
	default:
	  return "/* unknown wchar size */";
	  break;
	}
	break;
      case lt_string:
	switch(mpz_get_ui(s->v.i)) {
	case 0:
	case 32:
	  return "wstring";
	  break;
	case 8:
	  return "string";
	  break;
	default:
	  return "/* unknown string size */";
	  break;
	}
	break;
      case lt_float:
	switch(mpz_get_ui(s->v.i)) {
	case 32:
	  return "f32";
	  break;
	case 64:
	  return "f64";
	  break;
	case 128:
	  return "f128";
	  break;
	default:
	  return "/* unknown float size */";
	  break;
	}
	break;
      case lt_bool:
	{
	  return "bool";
	  break;
	}
      case lt_void:
	{
	  return "void";
	  break;
	}
      default:
	return "/* Bad primtype type code */";
      }
    }
  case sc_symRef:
    {
      return c_serializer(s->value);
      break;
    }
  case sc_interface:
  case sc_absinterface:
    {
      return "cap_t";
    }
  default:
    return symbol_QualifiedName(s,'_');
  }
}

/* c_byreftype is an optimization -- currently disabled, and needs
   debugging */
bool
c_byreftype(Symbol *s)
{
  return false;
#if 0
  switch(s->cls) {
  case sc_symRef:
    {
      return c_byreftype(s->value);
    }
  case sc_struct:
  case sc_union:
  case sc_seqType:		// Maybe
  case sc_arrayType:
    {
      return true;
    }


  default:
    return false;
  }
#endif
}

MP_INT
compute_value(Symbol *s)
{
  mpz_t value;

  switch(s->cls) {
  case sc_const:
    {
      return compute_value(s->value);
    }
  case sc_symRef:
    {
      return compute_value(s->value);
    }

  case sc_value:
    {
      switch(s->cls)
	{
	case lt_integer:
	case lt_unsigned:
	  return *s->v.i;

	default:
	  diag_fatal(1, "Constant computation with bad value type \"%s\"\n",
		      s->name);
	}
      break;
    }
    case sc_arithop:
      {
	MP_INT left;
	MP_INT right;
	char op;

	assert(! s->children.empty());

	mpz_init(&right);

	op = mpz_get_ui(s->v.i);

	left = compute_value(s->children[0]);
	if (s->children.size() == 1) {
	  assert(op == '-');	// Only minus can be unary
	  mpz_neg(&left, &left);
	  return left;
	}
	  
	right = compute_value(s->children[1]);

	switch(op) {
	  case '-':
	    {
	      mpz_sub(&left, &left, &right);
	      return left;
	    }
	  case '+':
	    {
	      mpz_add(&left, &left, &right);
	      return left;
	    }
	  case '*':
	    {
	      mpz_mul(&left, &left, &right);
	      return left;
	    }
	  case '/':
	    {
	      mpz_tdiv_q(&left, &left, &right);
	      return left;
	    }
	  default:
	    diag_fatal(1, "Unhandled arithmetic operator \"%s\"\n",
			s->name);
	}
      }
  default:
    {
      diag_fatal(1, "Busted arithmetic\n");
      break;
    }
  }

  mpz_init(value);
  return *value;
}

void
output_c_type_trailer(Symbol *s, FILE *out, int indent)
{
  s = symbol_ResolveRef(s);

  /* If this symbol is a typedef, we put out the typedef name
     as its type name, so don't expose the underlying type. */

  if (s->cls != sc_typedef) {
    s = symbol_ResolveType(s);
  }

#if 0
  /* FIX: This seems wrong to me. If it is truly a variable length
   * sequence type then this bound computation may not be statically
   * feasible. */
  if (symbol_IsVarSequenceType(s) || symbol_IsFixSequenceType(s)) {
    MP_INT bound = compute_value(s->value);

    // The bound on the size
    fprintf(out, "[%u]", mpz_get_ui(&bound));
  }
#endif
  if (symbol_IsFixSequenceType(s)) {
    MP_INT bound = compute_value(s->value);

    // The bound on the size
    fprintf(out, "[%u]", mpz_get_ui(&bound));
  }
}

static void
print_typedef(Symbol *s, FILE *out, int indent)
{
  print_asmifdef(out);
  do_indent(out, indent);
  fprintf(out, "typedef ");
  output_c_type(s->type, out, indent);
  fprintf(out, " %s", symbol_QualifiedName(s,'_'));
  output_c_type_trailer(s->type, out, 0);
  fprintf(out, ";\n");
  print_asmendif(out);
}

static void
symdump(Symbol *s, FILE *out, int indent)
{
  if (s->docComment)
    PrintDocComment(out, s->docComment, indent);

  switch(s->cls){
  case sc_package:
    {
      /* Don't output nested packages. */
      if (s->nameSpace->cls == sc_package)
	return;

      do_indent(out, indent);
      fprintf(out, "#ifndef __%s__ /* package %s */\n",
	      symbol_QualifiedName(s,'_'),
	      symbol_QualifiedName(s, '.'));
      fprintf(out, "#define __%s__\n",
	      symbol_QualifiedName(s,'_'));

      for (const auto eachChild : s->children)
	symdump(eachChild, out, indent + 2);

      fprintf(out, "\n#endif /* __%s__ */\n",
	      symbol_QualifiedName(s,'_'));
      break;
    }
  case sc_scope:
    {
      fputc('\n', out);
      do_indent(out, indent);
      fprintf(out, "/* namespace %s { */\n",
	      symbol_QualifiedName(s,'_'));

      for (const auto eachChild : s->children)
	symdump(eachChild, out, indent + 2);

      do_indent(out, indent);
      fprintf(out, "/* } ; */\n");

      break;
    }

    /*  case sc_builtin: */

    /*
     * NOTE:
     * CapIDL structs seem to be getting called as though they were
     * typedef structs so this checks to see if the case is for sc_struct
     * and adds a typedef accordingly.  It might also be possible to
     * add struct into the function calls, however, since all of the
     * calls don't include struct, it seems intuitively obvious that
     * this ought to add typedef.  I could be wrong.  Fortunately,
     * it's not hard to change
     */

  case sc_enum:
    {
      fprintf(out, "\n");

      print_typedef(s, out, indent);

      for (const auto eachChild : s->children)
	symdump(eachChild, out, indent + 2);
      break;
    }

  case sc_struct:
    {
      fprintf(out, "\n");
      print_asmifdef(out);
      do_indent(out, indent);

      fprintf(out, "typedef %s %s {\n",
	      symbol_ClassName(s),
	      symbol_QualifiedName(s,'_'));

      for (const auto eachChild : s->children)
	symdump(eachChild, out, indent + 2);

      do_indent(out, indent);
      fprintf(out, "} %s;\n",
	      symbol_QualifiedName(s,'_'));
      print_asmendif(out);
      break;
    }
  case sc_exception:
    {
      unsigned long sig = symbol_CodedName(s);

      fprintf(out, "#define RC_%s 0x%x\n",
	      symbol_QualifiedName(s,'_'),
	      sig);

      /* Exceptions only generate a struct definition if they 
	 actually have members. */
      if (! s->children.empty()) {
	fprintf(out, "\n");
	print_asmifdef(out);
	do_indent(out, indent);

	fprintf(out, "struct %s {\n",
		symbol_QualifiedName(s,'_'));

	for (const auto eachChild : s->children)
	  symdump(eachChild, out, indent + 2);

	do_indent(out, indent);
	fprintf(out, "} ;\n");
	print_asmendif(out);
      }

      break;
    }

  case sc_interface:
  case sc_absinterface:
    {
      unsigned opr_ndx=1;	// opcode 0 is (arbitrarily) reserved

      unsigned long sig = symbol_CodedName(s);

      fprintf(out, "\n");
      do_indent(out, indent);
      fprintf(out, "/* BEGIN Interface %s */\n",
	      symbol_QualifiedName(s,'.'));
      
      /* Identify the exceptions raised at the interface level first: */
      for (const auto eachRaised : s->raised) {
	do_indent(out, indent+2);
	fprintf(out, "/* interface raises %s */\n",
		eachRaised->QualifiedName('_') );
      }

      fprintf(out, "\n");

      fprintf(out, "#define IKT_%s 0x%x\n\n",
	      symbol_QualifiedName(s,'_'),
	      sig);

      do_indent(out, indent);
      print_asmifdef(out);
      fprintf(out,"typedef cap_t %s;\n",
	      symbol_QualifiedName(s,'_'));
      print_asmendif(out);
      fprintf(out,"\n");

      for (const auto eachChild : s->children) {
	if (eachChild->cls == sc_operation) {
	  if (eachChild->flags & SF_NO_OPCODE) {
	    fprintf(out, "\n/* Method %s is client-only */\n",
		    symbol_QualifiedName(eachChild,'_'),
		    ((s->ifDepth << 24) | opr_ndx++));
	  }
	  else {
	    fprintf(out, "\n#define OC_%s 0x%x\n",
		    symbol_QualifiedName(eachChild,'_'),
		    ((s->ifDepth << 24) | opr_ndx++));
	  }
	}

	symdump(eachChild, out, indent);
      }

      fputc('\n', out);

      do_indent(out, indent);
      fprintf(out, "/* END Interface %s */\n",
	      symbol_QualifiedName(s,'.'));

      break;
    }

  case sc_symRef:
    {
      fprintf(out, "/* symref %s */", s->name);
      fprintf(out, "%s", symbol_QualifiedName(s->value,'_'));
      break;
    }

  case sc_typedef:
    {
      print_typedef(s, out, indent);
      break;
    }

  case sc_union:
  case sc_caseTag:
  case sc_caseScope:
  case sc_oneway:
  case sc_switch:
    {
      do_indent(out, indent);
      fprintf(out, "/* cls %s %s */\n",
	      symbol_ClassName(s),
	      symbol_QualifiedName(s,'_'));

      break;
    }
  case sc_primtype:
    {
      do_indent(out, indent);
      output_c_type(s, out, indent);
      break;
    }
  case sc_operation:
    {
      assert(symbol_IsVoidType(s->type));

      print_asmifdef(out);
      do_indent(out, indent);
#if 0
      symdump(s->type, out, indent);
#else
      fprintf(out, "result_t");
#endif

      fprintf(out, " %s(cap_t _self", symbol_QualifiedName(s,'_'));

      for (const auto eachChild : s->children) {
	fprintf(out, ", ");
	symdump(eachChild, out, 0);
      }

      fprintf(out, ");\n");

      for (const auto eachRaised : s->raised) {
	do_indent(out, indent + 2);
	fprintf(out, "/* raises %s */\n" ,
		eachRaised->QualifiedName('_') );
      }

      print_asmendif(out);
      break;
    }

  case sc_arithop:
    {
      do_indent(out, indent);
      fputc('(', out);

      if (s->children.size() > 1) {
	symdump(s->children[0], out, indent + 2);
	fprintf(out, "%s", s->name);
	symdump(s->children[1], out, indent + 2);
      }
      else {
	fprintf(out, "%s", s->name);
	symdump(s->children[0], out, indent + 2);
      }
      fputc(')', out);

      break;
    }

  case sc_value:		/* computed constant values */
    {
      switch(s->v.lty){
      case lt_integer:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  fprintf(out, "%s", str);
	  free(str);
	  break;
	}
      case lt_unsigned:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  fprintf(out,"%su", str);
	  free(str);
	  break;
	}
      case lt_float:
	{
	  fprintf(out, "%f", s->v.d);
	  break;
	}
      case lt_char:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  fprintf(out, "'%s'", str);
	  free(str);
	  break;
	}
      case lt_bool:
	{
	  fprintf(out, "%s", (mpz_get_ui(s->v.i) ? "TRUE" : "FALSE"));
	  break;
	}
      default:
	{
	  fprintf(out, "/* lit UNKNOWN/BAD TYPE> */");
	  break;
	}
      };
      break;
    }

  case sc_const:		/* constant symbols, including enum values */
    {
      /* Issue: the C 'const' declaration declares a value, not a
	 constant. This is bad. Unfortunately, using #define does not
	 guarantee proper unification of string constants in the
	 string pool (or rather, it does according to ANSI C, but it
	 isn't actually implemented in most compilers). We therefore
	 use #define ugliness. We do NOT guard against multiple
	 definition here, since the header file overall already has
	 this guard. */
      /* FIX: Not all constants are integers. The call below to
	 mpz_get_ui needs to be replaced with a suitable type dispatch
	 on the legal literal types. */
      MP_INT mi = compute_value(s->value);
      fprintf(out, "#define %s ", symbol_QualifiedName(s,'_'));
      if (mpz_sgn(&mi) < 0)
	fprintf(out, "%d\n", mpz_get_si(&mi));
      else
	fprintf(out, "%u\n", mpz_get_si(&mi));
      break;
    }

  case sc_member:
    {
      do_indent(out, indent);
      output_c_type(s->type, out, indent);

      fprintf(out, " %s", s->name);

      output_c_type_trailer(s->type, out, 0);

      fprintf(out, ";\n");

      break;
    }
  case sc_ioformal:
    {
      FormalSym * fsym = dynamic_cast<FormalSym*>(s);
      assert(fsym);

      bool wantPtr = fsym->isOutput || c_byreftype(s->type);
      wantPtr = wantPtr && !symbol_IsInterface(s->type);

      output_c_type(s->type, out, 0);
      fputc(' ', out);
      if (wantPtr)
	fprintf(out, "*");
      fprintf(out, "%s", s->name);
      /* calling output_c_type_trailer here, leads to some of the same
	 problems we saw in o_c_client, where a [value] was getting
	 appended to a procedure declaration.  In this case it's not
	 correct, so for now, it has been disabled.

	 output_c_type_trailer(s->type, out, 0);
      */
      break;
    }

  case sc_seqType:
    {
      fprintf(out, "seqType SHOULD NOT BE OUTPUT DIRECTLY!\n");
      break;
    }

  case sc_arrayType:
    {
      fprintf(out, "arrayType SHOULD NOT BE OUTPUT DIRECTLY!\n");
      break;
    }

  default:
    {
      fprintf(out, "UNKNOWN/BAD SYMBOL TYPE %s FOR: %s\n",
	      s->cls, s->name);
      break;
    }
  }
}

void
output_c_hdr(Symbol *s)
{
  unsigned i;
  extern const char *target;
  InternedString fileName;
  char *dirName;
  char *slashPos;
  FILE *out;
  PtrVec *vec;
  
  if (s->isActiveUOC == false)
    return;

  {
    const char *sqn = symbol_QualifiedName(s, '/');
    char *tmp = VMALLOC(char, 
			strlen(target) + strlen("/") + strlen(sqn) 
			+ strlen(".h") + 1);
    
    strcpy(tmp, target);
    strcat(tmp, "/");
    strcat(tmp, sqn);
    strcat(tmp, ".h");
    fileName = intern(tmp);

    free(tmp);
  }

  dirName = strdup(fileName);
  slashPos = strrchr(dirName, '/');
  *slashPos = 0;

  path_smkdir(dirName);

  out = fopen(fileName, "w");
  if (out == NULL)
    diag_fatal(1, "Couldn't open stub source file \"%s\"\n",
	       fileName);

  vec = ptrvec_create();

  symbol_ComputeDependencies(s, vec);

  ptrvec_sort_using(vec, symbol_SortByQualifiedName);

  /* Header guard: */
  fprintf(out, "#ifndef __%s_h__\n", symbol_QualifiedName(s, '_'));
  fprintf(out, "#define __%s_h__\n", symbol_QualifiedName(s, '_'));

  /* 
   * We need target.h for long long to work, however, I'm not certain this
   * is the proper place for it to go.  This ought to work in the interim.
   */

  fputc('\n', out);
  print_asmifdef(out);
  fprintf(out, "#include <eros/target.h>\n");
  print_asmendif(out);

  for (i = 0; i < vec_len(vec); i++) {
    Symbol *sym = symvec_fetch(vec,i);

    fprintf(out, "#include <idl/%s.h>\n", symbol_QualifiedName(sym, '/'));
  }

  symdump(s, out, 0);

  /* final endif for header guards */
  fputc('\n', out);
  fprintf(out, "#endif /* __%s_h__ */\n", s->name);
}

static void
print_asmifdef(FILE *out)
{
  fprintf(out, "#ifndef __ASSEMBLER__\n");
}

static void
print_asmendif(FILE *out)
{
  fprintf(out, "#endif /* __ASSEMBLER__ */\n");
}
