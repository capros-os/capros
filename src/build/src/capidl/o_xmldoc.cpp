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

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/Intern.h>
#include <applib/Diag.h>
#include "SymTab.h"

static void
do_indent(int indent)
{
  int i;

  for (i = 0; i < indent; i ++)
    diag_printf(" ");
}

static void
PrintDocComment(const char *str, int indent)
{
  bool want_fill = false;

  do_indent(indent);

  diag_printf("<description>\n");

  do_indent(indent+2);
  
  while (*str) {
    if (want_fill) {
      do_indent(indent+2);
      want_fill = false;
    }

    if (*str == '\n')
      want_fill = true;
    diag_printf("%c", *str++);
  }

  do_indent(indent);
  diag_printf("</description>\n");
}

static void
symdump(Symbol *s, int indent)
{
  do_indent(indent);

  switch(s->cls){
  case sc_package:
#if 0
    /* Don't output nested packages. */
    if (s->nameSpace->cls == sc_package)
      return;
#endif

  case sc_scope:
  case sc_builtin:
  case sc_enum:
  case sc_struct:
  case sc_union:
  case sc_interface:
  case sc_absinterface:
  case sc_primtype:
  case sc_exception:
  case sc_caseTag:
  case sc_caseScope:
  case sc_oneway:
  case sc_typedef:
  case sc_switch:
  case sc_symRef:
    {
      if (!symbol_IsScope(s) ||
	  (s->children.empty() && !s->docComment && s->baseType == 0)) {
	diag_printf("<%s name=\"%s\" qname=\"%s\"/>\n", 
		     symbol_ClassName(s), s->name,
		     symbol_QualifiedName(s, '_'));
      }
      else {
	diag_printf("<%s name=\"%s\" qname=\"%s\">\n", symbol_ClassName(s), 
		     s->name, symbol_QualifiedName(s, '_'));

	if (s->docComment)
	  PrintDocComment(s->docComment, indent+2);

	if (s->baseType) {
	  do_indent(indent +2);
	  diag_printf("<extends qname=\"%s\"/>\n",
		       symbol_QualifiedName(s, '_'));
	}

	for (const auto eachChild : s->children)
	  symdump(eachChild, indent + 2);

  for (const auto eachRaised : s->raised)
    symdump(eachRaised, indent + 2);

	do_indent(indent);
	diag_printf("</%s>\n", symbol_ClassName(s));
      }
      break;
    }
  case sc_operation:
    {
      const char *type_name = s->type->name;

      bool use_nest = (! s->children.empty() || s->docComment);

      diag_printf("<%s name=\"%s\" qname=\"%s\" ty=\"%s\" tyclass=\"%s\"%s>\n", 
		   symbol_ClassName(s), s->name,
		   symbol_QualifiedName(s, '_'),
		   type_name, symbol_ClassName(s->type),
		   use_nest ? "" : "/");

      if (use_nest) {
	if (s->docComment)
	  PrintDocComment(s->docComment, indent+2);

	for (const auto eachChild : s->children)
	  symdump(eachChild, indent + 2);

  for (const auto eachRaised : s->raised)
    symdump(eachRaised, indent + 2);

	do_indent(indent);
	diag_printf("</%s>\n", symbol_ClassName(s));
      }

      break;
    }

  case sc_arithop:
    {
      diag_printf("<%s name=\"%s\" qname=\"%s\">\n", symbol_ClassName(s), 
		   s->name, symbol_QualifiedName(s, '_'));

      for (const auto eachChild : s->children)
	symdump(eachChild, indent + 2);

      do_indent(indent);
      diag_printf("</%s>\n", symbol_ClassName(s));

      break;
    }

  case sc_value:		/* computed constant values */
    {
      switch(s->v.lty){
      case lt_integer:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  diag_printf("<lit int %s/>\n", str);
	  free(str);
	  break;
	}
      case lt_unsigned:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  diag_printf("<lit uint %s>\n", str);
	  free(str);
	  break;
	}
      case lt_float:
	{
	  diag_printf("<lit float %f/>\n", s->v.d);
	  break;
	}
      case lt_char:
	{
	  char *str = mpz_get_str(NULL, 10, s->v.i);
	  diag_printf("<lit char \'%s\'/>\n", str);
	  free(str);
	  break;
	}
      case lt_bool:
	{
	  diag_printf("<lit bool %s/>\n", mpz_get_ui(s->v.i) ? "TRUE" : "FALSE");
	  break;
	}
      default:
	{
	  diag_printf("<lit UNKNOWN/BAD TYPE>\n");
	  break;
	}
      };
      break;
    }

  case sc_const:		/* constant symbols, including enum values */
    {
      const char *type_name = s->type->name;

      diag_printf("<%s name=\"%s\" qname=\"%s\" ty=\"%s\" tyclass=\"%s\">\n", 
		   symbol_ClassName(s), s->name,
		   symbol_QualifiedName(s, '_'),
		   type_name, symbol_ClassName(s->type));

      if (s->docComment)
	PrintDocComment(s->docComment, indent+2);

      symdump(s->value, indent+2);

      do_indent(indent);
      diag_printf("</%s>\n", symbol_ClassName(s));

      break;
    }

  case sc_member:
  case sc_ioformal:
    {
      const char *type_name = s->type->name;

      if (s->docComment) {
	diag_printf("<%s name=\"%s\" qname=\"%s\" ty=\"%s\" tyclass=\"%s\">\n", 
		     symbol_ClassName(s), s->name,
		     symbol_QualifiedName(s, '_'),
		     type_name, symbol_ClassName(s->type));

	PrintDocComment(s->docComment, indent+2);

	do_indent(indent);
	diag_printf("</%s>\n", symbol_ClassName(s));
      }
      else {
	diag_printf("<%s name=\"%s\" qname=\"%s\" ty=\"%s\" tyclass=\"%s\"/>\n", 
		     symbol_ClassName(s), s->name,
		     symbol_QualifiedName(s, '_'),
		     type_name, symbol_ClassName(s->type));
      }

      break;
    }

  case sc_seqType:
  case sc_arrayType:
    {
      diag_printf("<%s name=\"%s\">\n", symbol_ClassName(s), s->name);

      symdump(s->type, indent+2);
      if (s->value)
	symdump(s->value, indent+2);

      do_indent(indent);
      diag_printf("</%s>\n", symbol_ClassName(s));

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
output_xmldoc(Symbol *s)
{
  diag_printf("<!DOCTYPE obdoc SYSTEM \"../../../DTD/doc.dtd\">\n");
  diag_printf("<obdoc>\n");

  symdump(s, 2);

  diag_printf("</obdoc>\n");
}
