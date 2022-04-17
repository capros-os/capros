/*
 * Copyright (C) 2002, The EROS Group, LLC.
 * Copyright (C) 2022, Charles Landau.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/Intern.h>
#include <applib/Diag.h>
#include "SymTab.h"

static void
PrintDocComment(const char *str, int indent)
{
  int i;
  bool want_fill = false;

  for (i = 0; i < indent; i ++)
    diag_printf(" ");
  diag_printf("/**\n");

  for (i = 0; i < indent + 2; i ++)
    diag_printf(" ");
  
  while (*str) {
    if (want_fill) {
      for (i = 0; i < indent + 2; i ++)
	diag_printf(" ");
      want_fill = false;
    }

    if (*str == '\n')
      want_fill = true;
    diag_printf("%c", *str++);
  }

  for (i = 0; i < indent; i ++)
    diag_printf(" ");
  diag_printf("*/\n");
}

static void
do_indent(int indent)
{
  int i;

  for (i = 0; i < indent; i ++)
    diag_printf(" ");

}

static void
symdump(Symbol *s, int indent)
{
  if (s->docComment)
    PrintDocComment(s->docComment, indent+2);

  switch(s->cls){
    /* Deal with all of the scope-like elements: */
  case sc_package:
  case sc_scope:
  case sc_enum:
  case sc_struct:
  case sc_union:
  case sc_interface:
  case sc_absinterface:
    {
      if (s->cls == sc_absinterface)
	diag_printf("abstract ");

      diag_printf("%s %s ", symbol_ClassName(s), s->name);

      if (s->baseType)
	diag_printf("extends %s ", 
		     symbol_QualifiedName(s->baseType,'.'));

      if (! s->raised.empty()) {
	diag_printf("raises (");
	bool first = true;
	for (const auto eachRaised : s->raised) {
      	  if (!first)
	    diag_printf(", ");
          else
            first = false;
	  diag_printf(eachRaised->name);
	}

	diag_printf(") ");

	diag_fatal(1, "CapIDL dump of interface raises "
		    "is unimplemented \"%s\"\n",
		    s->name);
      }

      diag_printf("{\n");

      bool first = true;
      for (const auto eachChild : s->children) {
      	if (! first) {
	  if (s->cls == sc_enum)
	    diag_printf(",\n");
	}
      	else
      	  first = false;

	symdump((eachChild), indent + 2);
      }

      do_indent(indent);
      diag_printf("};\n", symbol_ClassName(s));

      break;
    }

  case sc_symRef:
    {
      diag_printf("%s ", s->name);

      break;
    }

  case sc_builtin:
  case sc_exception:
  case sc_caseTag:
  case sc_caseScope:
  case sc_oneway:
  case sc_typedef:
  case sc_switch:
    {
      if (!symbol_IsScope(s) ||
	  (s->children.empty() && !s->docComment && s->baseType == 0)) {
	diag_printf("<%s name=\"%s\" qname=\"%s\"/>\n", symbol_ClassName(s), 
		     s->name, symbol_QualifiedName(s,'_'));
      }
      else {
	diag_printf("<%s name=\"%s\" qname=\"%s\">\n", symbol_ClassName(s), 
		     s->name, symbol_QualifiedName(s,'_'));

	if (s->docComment)
	  PrintDocComment(s->docComment, indent+2);

	if (s->baseType) 
	  symdump(s->baseType, indent + 2);

	 for (const auto eachChild : s->children)
	  symdump((eachChild), indent + 2);

	do_indent(indent);
	diag_printf("</%s>\n", symbol_ClassName(s));
      }
      break;
    }
  case sc_primtype:
    {
      switch(s->v.lty){
      case lt_unsigned:
	diag_printf("unsigned ");
	/* Fall through */
      case lt_integer:
	switch(mpz_get_ui(s->v.i)) {
	case 0:
	  diag_printf("Integer");
	  break;
	case 8:
	  diag_printf("signed char");
	  break;
	case 16:
	  diag_printf("short");
	  break;
	case 32:
	  diag_printf("int");
	  break;
	case 64:
	  diag_printf("long long");
	  break;
	default:
	  diag_printf("/* unknown integer size */");
	  break;
	};

	break;
      case lt_char:
	switch(mpz_get_ui(s->v.i)) {
	case 0:
	case 32:
	  diag_printf("wchar_t");
	  break;
	case 8:
	  diag_printf("char");
	  break;
	default:
	  diag_printf("/* unknown wchar size */");
	  break;
	}
	break;
      case lt_string:
	switch(mpz_get_ui(s->v.i)) {
	case 0:
	case 32:
	  diag_printf("wstring");
	  break;
	case 8:
	  diag_printf("string");
	  break;
	default:
	  diag_printf("/* unknown string size */");
	  break;
	}
	break;
      case lt_float:
	switch(mpz_get_ui(s->v.i)) {
	case 32:
	  diag_printf("float");
	  break;
	case 64:
	  diag_printf("double");
	  break;
	case 128:
	  diag_printf("long double");
	  break;
	default:
	  diag_printf("/* unknown float size */");
	  break;
	}
	break;
      case lt_bool:
	{
	  diag_printf("bool");
	  break;
	}
      case lt_void:
	{
	  diag_printf("void");
	  break;
	}
      default:
	diag_printf("/* Bad primtype type code */");
      }
      break;
    }
  case sc_operation:
    {
      do_indent(indent);
      symdump(s->type, indent);

      diag_printf(" %s(", s->name);

      bool first = true;
      for (const auto eachChild : s->children) {
      	if (!first)
	  diag_printf(",\n");
      	else
      	  first = false;
	symdump((eachChild), 0);
      }

      if (! s->raised.empty()) {
	diag_printf(")\n");
	do_indent(indent + 2);
	diag_printf("(");
        first = true;
	for (const auto eachRaised : s->raised) {
      	  if (!first)
	    diag_printf(", ");
          else
            first = false;
	  diag_printf("%s", symbol_QualifiedName(eachRaised,'_'));
	}
        diag_printf(");\n");
      }

      break;
    }

  case sc_arithop:
    {
      do_indent(indent);
      diag_printf("(");

      if (s->children.size() > 1) {
	symdump(s->children[0], indent + 2);
	diag_printf(s->name);
	symdump(s->children[1], indent + 2);
      }
      else {
	diag_printf(s->name);
	symdump(s->children[0], indent + 2);
      }
      diag_printf(")");

      break;
    }

  case sc_value:		/* computed constant values */
    {
      switch(s->v.lty){
      case lt_integer:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  diag_printf("%s", str);
	  free(str);
	  break;
	}
      case lt_unsigned:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  diag_printf("%su", str);
	  free(str);
	  break;
	}
      case lt_float:
	{
	  diag_printf("%f\n", s->v.d);
	  break;
	}
      case lt_char:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  diag_printf("'%s'", str);
	  free(str);
	  break;
	}
      case lt_bool:
	{
	  diag_printf("%s\n", mpz_get_ui(s->v.i) ? "TRUE" : "FALSE");
	  break;
	}
      default:
	{
	  diag_printf("/* lit UNKNOWN/BAD TYPE> */");
	  break;
	}
      };
      break;
    }

  case sc_const:		/* constant symbols, including enum values */
    {
      do_indent(indent);

      if (s->nameSpace->cls != sc_enum)
	symdump(s->type, indent);

      diag_printf(" %s = ", s->name);
      symdump(s->value, 0);

      if (s->nameSpace->cls != sc_enum)
	diag_printf(";\n");
      break;
    }

  case sc_member:
    {
	do_indent(indent);

	symdump(s->type, indent+2);
	diag_printf("%s;\n", s->name);

	break;
    }

  case sc_formal:
  case sc_outformal:
    {
      if (s->cls == sc_outformal)
	diag_printf("OUT ");

      symdump(s->type, 0);

      diag_printf("%s", s->name);
      break;
    }

  case sc_seqType:
    {
      symdump(s->type, 0);
      diag_printf("[*");
      if (s->value)
	symdump(s->value, 0);
      diag_printf("]");

      break;
    }

  case sc_arrayType:
    {
      symdump(s->type, 0);
      diag_printf("[");
      symdump(s->value, 0);
      diag_printf("]");

      break;
    }

  default:
    {
      diag_printf("UNKNOWN/BAD SYMBOL TYPE %d FOR: %s", 
		   s->cls, s->name);
      break;
    }
  }
}

void
output_capidl(Symbol *s)
{
  diag_printf("package %s;\n\n", symbol_QualifiedName(s->nameSpace,'.'));

  symdump(s, 0);
}
