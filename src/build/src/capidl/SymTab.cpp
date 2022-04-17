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
#include <applib/sha1.h>
#include <applib/xmalloc.h>
#include "SymTab.h"
#include "util.h"

/** Following scopes are universal, in the sense that they are 
    shared across all input processing. The /UniversalScope/ is the
    containing scope for all of the package scopes we will be
    building. Each imported package in turn has a top-level package
    scope.
*/
/** Universal package scope.
    
    This scope object contains all of the packages that are loaded in
    a given execution of capidl. The general processing strategy of
    capidl is to process all of the input units of compilation and
    then export header/implementation files (according to the command
    line options) for each package found under UniversalScope at the
    end of processing. 
*/
    
Symbol *symbol_UniversalScope = 0;

/*** 
     Scope for builtin types. 

     Several of the symbols here are internally defined type symbols
     that are proxy objects for the builtin types. All of these
     symbols begin with "#", in order to ensure that they will not
     match any of the builtin lookup predicates.

     The keyword scope probably ought to be replaced by a CapIDL
     package. It then becomes an interesting question whether builtin
     type names such as int, long should be overridable.
 */
Symbol *symbol_KeywordScope = 0; /* scope for keywords */ 
Symbol *symbol_voidType = 0;	/* void type symbol */ 
Symbol *symbol_curScope = 0;

const char *symbol_sc_names[] = {
#define SYMCLASS(x,n) #x,
#include "symclass.def"
#undef  SYMCLASS
};

unsigned symbol_sc_isScope[] = {
#define SYMCLASS(x,n) n,
#include "symclass.def"
#undef  SYMCLASS
};

static void symbol_AddChild(Symbol *parent, Symbol *sym);

void
symbol_PushScope(Symbol *newScope)
{
  assert (symbol_curScope == 0 || newScope->nameSpace == symbol_curScope);

  symbol_curScope = newScope;
}

void
symbol_PopScope()
{
  assert(symbol_curScope);

  symbol_curScope->complete = true;

  symbol_curScope = symbol_curScope->nameSpace;
}

Symbol*
symbol_LookupChild(Symbol *s, const char *nm, Symbol *bound)
{
  Symbol *child;
  Symbol *childScope;
  InternedString ident;
  InternedString rest;

  /* If /nm/ is not a qualified name, then it should be an immediate
     descendant: */

  nm = intern(nm);
  const char * dot = strchr(nm, '.');


  if (dot == 0) {
    unsigned i;
    for (i = 0; i < vec_len(s->children); i++) {
      child = symvec_fetch(s->children,i);

      if (bound && bound == child)
	return 0;

      if (child->name == nm)
	return child;
    }

    return 0;
  }
  else {
    ident = internWithLength(nm, dot - nm);
    rest = intern(dot+1);

    childScope = symbol_LookupChild(s, ident, bound);
    if (childScope) 
      return symbol_LookupChild(childScope, rest, 0);
    return 0;
    
  }
}

Symbol *
symbol_construct(const char *nm, bool isActiveUOC, SymClass sc)
{
  return new Symbol(nm, isActiveUOC, sc);
}
Symbol::Symbol(const char *nm, bool isActiveUOC, SymClass sc) :
  name(intern(nm)),
  isActiveUOC(isActiveUOC),
  cls(sc),
  complete(symbol_sc_isScope[sc] ? true : false),
  children(ptrvec_create())
{
  v.lty = lt_void;
  mpz_init(v.i);
}

Symbol *
symbol_create_inScope(const char * nm, bool isActiveUOC, SymClass sc, Symbol *scope)
{
  Symbol *sym;
  
  nm = intern(nm);

  if (scope) {
    sym = symbol_LookupChild(scope, nm, 0);

    if (sym)
      return 0;
  }
  
  sym = symbol_construct(nm, isActiveUOC, sc);

  if (scope) {
    sym->nameSpace = scope;
    symbol_AddChild(scope, sym);
  }

  return sym;
}

Symbol *
symbol_create(const char *nm, bool isActiveUOC, SymClass sc)
{
  return symbol_create_inScope(nm, isActiveUOC, sc, symbol_curScope);
}

Symbol *
symbol_createRef_inScope(const char * nm, bool isActiveUOC, Symbol *inScope)
{
  Symbol *sym = symbol_construct(nm, isActiveUOC, sc_symRef);
  sym->type = 0;
  sym->nameSpace = inScope;

  return sym;
}

Symbol *
symbol_createRef(const char *nm, bool isActiveUOC)
{
  return symbol_createRef_inScope(nm, isActiveUOC, symbol_curScope);
}

Symbol *
symbol_createRaisesRef(const char *nm, bool isActiveUOC)
{
  nm = intern(nm);

  Symbol * const inScope = symbol_curScope;
  assert(inScope);

  /* If this exception has already been added, do not add it again. */

  /* FIX: What about raises inheritance in method overrides? */

  for (const auto eachRaised : inScope->raised) {
    if (eachRaised->name == nm)
      return 0;
  }

  Symbol * const sym = symbol_createRef_inScope(nm, isActiveUOC, inScope);

  /* The reference per se does not have a namespace */
  inScope->raised.push_back(sym);

  return sym;
}

/**
 * Because of the namespace design of the IDL language, it is possible
 * for one interface A to publish type ty (thus A.ty) and for a second
 * interface B to contain an operator that makes reference to this
 * type by specifying it as "A.ty". This usage is perfectly okay, but
 * in order to output the necessary header file dependencies it is
 * necessary that the generated compilation unit for B import the
 * generated compilation unit for A.
 *
 * Catch 1: The B unit of compilation may internally define a
 * namespace A that *also* defines a type ty. If so, this definition
 * "wins". We therefore need to do symbol resolution 
 *
 * Catch 2: An interface can "extend" another interface. Thus. when
 * resolving name references within an interface we need to consider
 * names published by "base" interfaces.
 *
 * Catch 3: Package names have no inherent lexical ordering. In
 * consequence, units of compilation also have no inherent lexical
 * ordering.
 *
 * Therefore, our symbol reference resolution strategy must proceed as
 * follows:
 * 
 *   1. Perform a purely lexical lookup looking for a name resolution
 *      that lexically preceeds the name we are attempting to
 *      resolve.
 *
 *      The last (top) scope we will consider is the children of the
 *      unit of compilation (and the unit of compilation itself, of
 *      course). 
 *
 *      During the course of lexical resolution of interfaces, we will
 *      (eventually) traverse base interfaces. It is not immediately
 *      clear (to me) how much of the lexical context of a base
 *      interface is inherited, so THIS IS NOT CURRENTLY IMPLEMENTED.
 *
 *   2. If no resolution has been achieved by (1), attempt a top-level
 *      name resolution relative to the universal scope. Note that
 *      this resolution may result in a circular dependency between
 *      units of compilation.
 */
Symbol*
symbol_LexicalLookup(Symbol *s, const char *nm, Symbol *bound)
{
  Symbol *sym = 0;
  Symbol *scope = s;

  nm = intern(nm);

  while (scope) {
    /* It is wrong to use the /bound/ when the scope symbol is an
       sc_package symbol, because units of compilation are not
       necessarily introduced into their containing package in lexical
       order.

       A circular dependency at this level must be caught in the
       circular dependency check between units of compilation that is
       performed as a separate pass. */
    if (scope->cls == sc_package)
      bound = 0;

    sym = symbol_LookupChild(scope, nm, bound);
    if (sym)
      break;

    bound = scope;
    scope = scope->nameSpace;

    /* Packages aren't really lexically nested, so do not consider
       metapackage a lexical scope: */
    if (scope && bound &&
	scope->cls == sc_package &&
	bound->cls == sc_package)
      scope = 0;
  }

  return sym;
}

bool
symbol_ResolveSymbolReference(Symbol *s)
{
  Symbol *sym;
  assert (s->cls == sc_symRef);

  sym = symbol_LexicalLookup(s->nameSpace, s->name, 0);

  if (sym == 0)
    sym = symbol_LookupChild(symbol_UniversalScope, s->name, 0);

  if (sym == 0)
    diag_printf("Unable to resolve symbol \"%s\"\n", s->name);

  s->value = sym;

  return s->value ? true : false;
}

Symbol *
symbol_createPackage(const char *nm, Symbol *inPkg)
{
  Symbol *thePkg;

  nm = intern(nm);
  const char * dot = strchr(nm,'.');

  if (dot == 0) {
    /* We are down to the tail identifier. */
    thePkg = symbol_LookupChild(inPkg, nm, 0);
    if (thePkg) {
      if (thePkg->cls == sc_package)
	return thePkg;
      return 0;
    }
    else
      return symbol_create_inScope(nm, false, sc_package, inPkg);
  }
  else {
    InternedString ident = internWithLength(nm, dot - nm);
    InternedString rest = intern(dot + 1);

    inPkg = symbol_createPackage(ident, inPkg);
    if (inPkg == 0)
      return inPkg;		// fail

    return symbol_createPackage(rest, inPkg);
  }
}

Symbol *
symbol_gensym(SymClass sc, bool isActiveUOC)
{
  return symbol_gensym_inScope(sc, isActiveUOC, symbol_curScope);
}

Symbol *
symbol_gensym_inScope(SymClass sc, bool isActiveUOC, Symbol *inScope)
{
  static int gensymcount = 0;
  char buf[20];
  sprintf(buf, "#anon%d", gensymcount++);

  return symbol_create_inScope(buf, isActiveUOC, sc, inScope);
}

Symbol *
symbol_MakeIntLit(const char *nm)
{
  Symbol *sym = symbol_construct(nm, true, sc_value);
  sym->v.lty = lt_integer;
  mpz_set_str(sym->v.i, nm, 0);

  return sym;
}

Symbol *
symbol_MakeMinLit(Symbol *s)
{
  assert(s->cls == sc_primtype);
  assert(s->v.lty == lt_integer);

  if ( s->name == intern("#int8") )
    return symbol_MakeIntLit(intern("-128"));
  if ( s->name == intern("#int16") )
    return symbol_MakeIntLit(intern("-32768"));
  if ( s->name == intern("#int32") )
    return symbol_MakeIntLit(intern("-2147483648"));
  if ( s->name == intern("#int64") )
    return symbol_MakeIntLit(intern("-9223372036854775808"));

  if ( s->name == intern("#uint8") )
    return symbol_MakeIntLit(intern("0"));
  if ( s->name == intern("#uint16") )
    return symbol_MakeIntLit(intern("0"));
  if ( s->name == intern("#uint32") )
    return symbol_MakeIntLit(intern("0"));
  if ( s->name == intern("#uint64") )
    return symbol_MakeIntLit(intern("0"));

  if ( s->name == intern("#wchar8") )
    return symbol_MakeIntLit(intern("0"));
  if ( s->name == intern("#wchar16") )
    return symbol_MakeIntLit(intern("0"));
  if ( s->name == intern("#wchar32") )
    return symbol_MakeIntLit(intern("0"));

  return 0;			/* no min/max */
}

Symbol *
symbol_MakeMaxLit(Symbol *s)
{
  assert(s->cls == sc_primtype);
  assert(s->v.lty == lt_integer ||
	 s->v.lty == lt_unsigned);
  /* Max char lits not supported yet! */

  if ( s->name == intern("#int8") )
    return symbol_MakeIntLit(intern("127"));
  if ( s->name == intern("#int16") )
    return symbol_MakeIntLit(intern("32767"));
  if ( s->name == intern("#int32") )
    return symbol_MakeIntLit(intern("2147483647"));
  if ( s->name == intern("#int64") )
    return symbol_MakeIntLit(intern("9223372036854775807"));

  if ( s->name == intern("#uint8") )
    return symbol_MakeIntLit(intern("255"));
  if ( s->name == intern("#uint16") )
    return symbol_MakeIntLit(intern("65535"));
  if ( s->name == intern("#uint32") )
    return symbol_MakeIntLit(intern("4294967295"));
  if ( s->name == intern("#uint64") )
    return symbol_MakeIntLit(intern("18446744073709551615"));

#if 0
  // FIX: There is a serious encoding problem here. These need to come
  // out as character literals.
  if ( s->name == intern("#wchar8") )
    return symbol_MakeIntLit(intern("255"));
  if ( s->name == intern("#wchar16") )
    return symbol_MakeIntLit(intern("65535"));
  if ( s->name == intern("#wchar32") )
    return symbol_MakeIntLit(intern("4294967295"));
#endif

  return 0;			/* no min/max */
}

Symbol *
symbol_MakeIntLitFromMpz(const mpz_t mp)
{
  char *nm = mpz_get_str(NULL, 0, mp);

  Symbol *sym = symbol_construct(nm, true, sc_value);
  sym->v.lty = lt_integer;
  mpz_set(sym->v.i, mp);

  free(nm);

  return sym;
}

Symbol *
symbol_MakeStringLit(const char *nm)
{
  Symbol *sym = symbol_construct(nm, true, sc_value);
  sym->v.lty = lt_string;

  return sym;
}

Symbol *
symbol_MakeFloatLit(const char *nm)
{
  Symbol *sym = symbol_construct(nm, true, sc_value);
  sym->v.lty = lt_float;
  sym->v.d = strtod(nm, NULL);

  return sym;
}

Symbol *
symbol_MakeCharLit(const char *nm)
{
  Symbol *sym = symbol_construct(nm, true, sc_value);
  sym->v.lty = lt_char;
  mpz_set_str(sym->v.i, nm, 0);

  return sym;
}

Symbol *
symbol_MakeKeyword(const char *nm, SymClass sc,
		    LitType lt,
		    unsigned value)
{
  Symbol *sym = symbol_construct(nm, false, sc);
  sym->nameSpace = symbol_KeywordScope;
  sym->v.lty = lt;
  mpz_set_ui(sym->v.i, value);
  symbol_AddChild(symbol_KeywordScope, sym);

  return sym;
}

Symbol *
symbol_MakeExprNode(const char *op, Symbol *left, Symbol *right)
{
  Symbol *sym = symbol_construct(op, false, sc_arithop);
  sym->nameSpace = symbol_UniversalScope;
  sym->v.lty = lt_char;
  mpz_set_ui(sym->v.i, *op);

  ptrvec_append(sym->children, left);
  ptrvec_append(sym->children, right);

  return sym;
}

static Symbol *int_types[9] = {
  0,				/* int<0> */
  0,				/* int<8> */
  0,				/* int<16> */
  0,				/* int<24> */
  0,				/* int<32> */
  0,				/* int<40> */
  0,				/* int<48> */
  0,				/* int<56> */
  0,				/* int<64> */
};

static Symbol *uint_types[9] = {
  0,				/* uint<0> */
  0,				/* uint<8> */
  0,				/* uint<16> */
  0,				/* uint<24> */
  0,				/* uint<32> */
  0,				/* uint<40> */
  0,				/* uint<48> */
  0,				/* uint<56> */
  0,				/* uint<64> */
};

Symbol *
symbol_LookupIntType(unsigned bitsz, bool uIntType)
{
  if (bitsz % 8)
    return 0;
  if (bitsz > 64)
    return 0;
  
  bitsz /= 8;

  return uIntType ? uint_types[bitsz] : int_types[bitsz];
}

void
symbol_InitSymtab()
{
  Symbol *sym;
  Symbol *boolType;

  symbol_UniversalScope = symbol_construct("<UniversalScope>", false, sc_scope);
  symbol_KeywordScope = symbol_construct("<KeywordScope>", false, sc_scope);

  /* Primitive types. These all start with '.' to ensure that they
   * cannot be successfully matched as an identifier, since the
   * "names" of these types are purely for my own convenience in doing
   * "dump" operations on the symbol table.
   */

  // Booleans are encoded in 8 bits.
  boolType = symbol_MakeKeyword("#bool", sc_primtype, lt_bool, 8);

  symbol_MakeKeyword("#int", sc_primtype, lt_integer, 0);
  symbol_MakeKeyword("#int8", sc_primtype, lt_integer, 8);
  symbol_MakeKeyword("#int16", sc_primtype, lt_integer, 16);
  symbol_MakeKeyword("#int32", sc_primtype, lt_integer, 32);
  symbol_MakeKeyword("#int64", sc_primtype, lt_integer, 64);

  symbol_MakeKeyword("#uint8", sc_primtype, lt_unsigned, 8);
  symbol_MakeKeyword("#uint16", sc_primtype, lt_unsigned, 16);
  symbol_MakeKeyword("#uint32", sc_primtype, lt_unsigned, 32);
  symbol_MakeKeyword("#uint64", sc_primtype, lt_unsigned, 64);

  symbol_MakeIntLit("0");		/* min unsigned anything */

  symbol_MakeIntLit("127");		/* max pos signed 8 bit */
  symbol_MakeIntLit("-128");		/* max neg signed 8 bit */ 
  symbol_MakeIntLit("255");		/* max unsigned 8 bit */
    
  symbol_MakeIntLit("32767");		/* max pos signed 16 bit */
  symbol_MakeIntLit("-32768");		/* max neg signed 16 bit */ 
  symbol_MakeIntLit("65535");		/* max unsigned 16 bit */
    
  symbol_MakeIntLit("2147483647");	/* max pos signed 32 bit */
  symbol_MakeIntLit("-2147483648");	/* max neg signed 32 bit */ 
  symbol_MakeIntLit("4294967295");	/* max unsigned 32 bit */
    
  symbol_MakeIntLit("9223372036854775807");	/* max pos signed 64 bit */
  symbol_MakeIntLit("-9223372036854775808");	/* max neg signed 64 bit */ 
  symbol_MakeIntLit("18446744073709551615");	/* max unsigned 64 bit */
    
  symbol_MakeKeyword("#float32", sc_primtype, lt_float, 32);
  symbol_MakeKeyword("#float64", sc_primtype, lt_float, 64);
  symbol_MakeKeyword("#float128", sc_primtype, lt_float, 128);

  symbol_MakeKeyword("#wchar8", sc_primtype, lt_char, 8);
  symbol_MakeKeyword("#wchar16", sc_primtype, lt_char, 16);
  symbol_MakeKeyword("#wchar32", sc_primtype, lt_char, 32);

  symbol_MakeKeyword("#wstring8", sc_primtype, lt_string, 8);
  symbol_MakeKeyword("#wstring32", sc_primtype, lt_string, 32);

  symbol_voidType = symbol_MakeKeyword("#void", sc_primtype, lt_void, 0);

  sym = symbol_MakeKeyword("true", sc_builtin, lt_bool, 1);
  sym->type = boolType;

  sym = symbol_MakeKeyword("false", sc_builtin, lt_bool, 0);
  sym->type = boolType;

  sym->type = boolType;
}

void
symbol_AddChild(Symbol *parent, Symbol *sym)
{
  ptrvec_append(parent->children, sym);
}

Symbol*
symbol_FindPackageScope()
{
  Symbol *scope = symbol_curScope;
  
  while (scope && (scope->cls != sc_package))
    scope = scope->nameSpace;

  return scope;
}

InternedString
symname_join(const char *n1, const char *n2, char sep)
{
  InternedString nm;
  unsigned len = strlen(n1) + 1 + strlen(n2) + 1;
  char *s = VMALLOC(char, len);
  char *sEnd;
  strcpy(s, n1);
  sEnd = s + strlen(s);
  *sEnd++ = sep;
  *sEnd++ = 0;	   /* nul */
  strcat(s, n2);

  nm = intern(s);
  free(s);

  return nm;
}

Symbol *symbol_ResolveType(Symbol *sym)
{
  if (symbol_IsBasicType(sym))
    return sym;
      
  switch(sym->cls) {
  case sc_formal:
  case sc_outformal:
  case sc_member:
  case sc_operation:
  case sc_oneway:
    assert(false);    
    
  case sc_typedef:
    return symbol_ResolveType(sym->type);
  case sc_symRef:
    return symbol_ResolveType(symbol_ResolveRef(sym));
  case sc_exception:
    return sym;
  default:
    return 0;
  }
}

InternedString
symbol_QualifiedName(Symbol *s, char sep)
{
  return s->QualifiedName(sep);
}
InternedString
Symbol::QualifiedName(char sep)
{
  if (symbol_IsAnonymous(this))
    return "";

  Symbol * sym = this;
  InternedString nm = sym->name;

  while (sym->nameSpace && sym->nameSpace != symbol_UniversalScope) {
    sym = sym->nameSpace;
    if (!symbol_IsAnonymous(sym))
      nm = symname_join(sym->name, nm, sep);
  }

  return nm;
}

unsigned long long
symbol_CodedName(Symbol *sym)
{
  unsigned long long ull;

  InternedString is = symbol_QualifiedName(sym, '.');
  const char *s = is;

  SHA *sha = sha_create();

  sha_append(sha, strlen(s), s);

  ull = sha_signature64(sha);

  free(sha);

  return ull;
}

#ifdef SYMDEBUG
void
symbol_QualifyNames(Symbol *sym)
{
  unsigned i;

  if (sym->cls == sc_symRef || sym->cls == sc_value || sym->cls == sc_builtin)
    return;			// skip these!

  sym->qualifiedName = symbol_QualifiedName(sym, '.');

  if (sym->baseType)
    symbol_QualifyNames(sym->baseType);
  if (sym->type)
    symbol_QualifyNames(sym->type);
  if (sym->value)
    symbol_QualifyNames(sym->value);

  for(i = 0; i < vec_len(sym->children); i++)
    symbol_QualifyNames(symvec_fetch(sym->children,i));

  for (const auto eachRaised : sym->raised)
    symbol_QualifyNames(eachRaised);
}
#endif

bool
symbol_ResolveReferences(Symbol *sym)
{
  unsigned i;
  bool result;

  if (sym->cls == sc_symRef)
    return symbol_ResolveSymbolReference(sym);

  if (sym->cls == sc_value || sym->cls == sc_builtin)
    return true;

  result = true;

  /* It is imperative that the baseType (if any) be resolved before
     any child resolutions are attempted, because lexical lookups to
     resolve the children may require traversal of the basetype. */
  if (sym->baseType)
    result = result &&  symbol_ResolveReferences(sym->baseType);
  if (sym->type)
    result = result && symbol_ResolveReferences(sym->type);
  if (sym->value)
    result = result && symbol_ResolveReferences(sym->value);

  for(i = 0; i < vec_len(sym->children); i++)
    result = result && symbol_ResolveReferences(symvec_fetch(sym->children,i));

  for (const auto eachRaised : sym->raised)
    result = result && symbol_ResolveReferences(eachRaised);

  return result;
}

void
symbol_ResolveIfDepth(Symbol *sym)
{
  unsigned i;
  if (sym->baseType)
    symbol_ResolveIfDepth(sym->baseType);

  if (sym->cls == sc_interface || sym->cls == sc_absinterface) {
    if (sym->ifDepth)		/* non-zero indicates already resolved */
      return;

    sym->ifDepth = sym->baseType ? sym->baseType->ifDepth + 1 : 1;
  }

  if (sym->type)
    symbol_ResolveIfDepth(sym->type);
  if (sym->value) {
    symbol_ResolveIfDepth(sym->value);
    sym->ifDepth = sym->value->ifDepth;
  }

  for(i = 0; i < vec_len(sym->children); i++)
    symbol_ResolveIfDepth(symvec_fetch(sym->children,i));

  for (const auto eachRaised : sym->raised)
    symbol_ResolveIfDepth(eachRaised);
}

bool
symbol_TypeCheck(Symbol *sym)
{
  unsigned i;
  bool result = true;

  if (sym->baseType && ! symbol_IsInterface(sym->baseType)) {
    diag_printf("Interface \"%s\" extends \"%s\", "
		 "which is not an interface type\n", 
		 symbol_QualifiedName(sym, '.'),
		 sym->baseType->name);
    result = false;
  }

  if (sym->type && symbol_IsType(sym->type, sc_seqType)) {
    diag_printf("%s \"%s\" specifies sequence type  \"%s\" (%s), "
		 "which is not yet supported\n", 
		 symbol_ClassName(sym),
		 symbol_QualifiedName(sym, '.'),
		 sym->type->name,
		 symbol_ClassName(sym->type));
    result = false;
  }

  if (sym->value && ! symbol_IsConstantValue(sym->value)) {
    diag_printf("Symbol \"%s\" is not a constant value\n",
		 sym->type->name);
    result = false;
  }

  if (! sym->raised.empty()) {
    if (!symbol_IsInterface(sym) && !symbol_IsOperation(sym)) {
      diag_printf("Exceptions can only be raised by interfaces and methods\n",
		   sym->name);
      result = false;
    }

    for (const auto eachRaised : sym->raised) {
      assert(eachRaised->cls == sc_symRef);

      if (! symbol_IsException(eachRaised)) {
	diag_printf("%s \"%s\" raises  \"%s\" (%s), "
		     "which is not an exception type\n", 
		     symbol_ClassName(sym),
		     symbol_QualifiedName(sym, '.'),
		     symbol_QualifiedName(eachRaised, '_'),
		     symbol_ClassName(eachRaised));
	result = false;
      }
    }
  }

  if (sym->cls == sc_formal && !symbol_IsValidParamType(sym->type)) {
    diag_printf("%s \"%s\" has illegal parameter type (hint: sequence<> and buffer<> should be typedefed)\n", 
		symbol_ClassName(sym),
		symbol_QualifiedName(sym, '.'));
    result = false;
  }

  if (sym->cls == sc_outformal && !symbol_IsValidParamType(sym->type)) {
    diag_printf("%s \"%s\" has illegal parameter type (hint: sequence<> and buffer<> should be typedefed)\n", 
		symbol_ClassName(sym),
		symbol_QualifiedName(sym, '.'));
    result = false;
  }

  if (sym->cls == sc_member && !symbol_IsValidMemberType(sym->type)) {
    diag_printf("%s \"%s\" has an illegal type (buffers cannot be structure/unino members))\n", 
		symbol_ClassName(sym),
		symbol_QualifiedName(sym, '.'));
    result = false;
  }

  if (sym->cls == sc_seqType && !symbol_IsValidSequenceBaseType(sym->type)) {
    diag_printf("%s \"%s\" has an illegal base type (sequences of buffers make no sense))\n", 
		symbol_ClassName(sym),
		symbol_QualifiedName(sym, '.'));
    result = false;
  }

  if (sym->cls == sc_bufType && !symbol_IsValidBufferBaseType(sym->type)) {
    diag_printf("%s \"%s\" has an illegal base type (buffers of sequences are currently not permitted))\n", 
		symbol_ClassName(sym),
		symbol_QualifiedName(sym, '.'));
    result = false;
  }

  if (sym->cls == sc_symRef)
    return result;

  for(i = 0; i < vec_len(sym->children); i++)
    result = result && symbol_TypeCheck(symvec_fetch(sym->children,i));

  return result;
}

Symbol *
symbol_UnitOfCompilation(Symbol *sym)
{
  if (sym->cls == sc_package)
    return 0;
  if (sym->nameSpace->cls == sc_package)
    return sym;
  return symbol_UnitOfCompilation(sym->nameSpace);
}

#if 0
bool
symbol_ResolveDepth()
{
  int myDepth = 0;

  if (cls == sc_value || cls == sc_builtin || cls == sc_primtype) {
    depth = 0;
    return true;
  }

  /* A depth value of -1 signals a symbol whose depth resolution is in
     progress. If we are asked to resolve the depth of such a symbol,
     there is a circular dependency. */

  if (depth == -1) {
    if (cls != sc_symRef) diag_printf("Symbol \"%s\"\n", QualifiedName('.'));
    return false;
  }

  depth = -1;			// unknown until otherwise proven

  if (baseType && !baseType->ResolveDepth()) {
    if (cls != sc_symRef) diag_printf("Symbol \"%s\"\n", QualifiedName('.'));
    return false;
  }
  if (type && !type->IsReferenceType() && !type->ResolveDepth()) {
    if (cls != sc_symRef) diag_printf("Symbol \"%s\"\n", QualifiedName('.'));
    return false;
  }
  if (value && !value->ResolveDepth()) {
    if (cls != sc_symRef) diag_printf("Symbol \"%s\"\n", QualifiedName('.'));
    return false;
  }

  if (baseType) myDepth = max(myDepth, baseType->depth);
  if (type && !type->IsReferenceType()) myDepth = max(myDepth, type->depth);
  if (value) myDepth = max(myDepth, value->depth);

  for(Symbol *child = children; child; child = child->next) {
    if (!child->ResolveDepth()) {
      if (cls != sc_symRef) diag_printf("Symbol \"%s\"\n", QualifiedName('.'));
      return false;
    }

    myDepth = max(myDepth, child->depth);
  }

  depth = myDepth + 1;

  return true;
}

void
symbol_ClearDepth()
{
  if (depth == -2)
    return;

  depth = -2;

  if (baseType) baseType->ClearDepth();
  if (type) type->ClearDepth();
  if (value) value->ClearDepth();

  for(Symbol *child = children; child; child = child->next)
    child->ClearDepth();
}
#endif

bool
symbol_IsLinearizable(Symbol *sym)
{
  unsigned i;

  if (sym->mark) {
    diag_printf("Symbol \"%s\"\n", symbol_QualifiedName(sym, '.'));
    return false;
  }

  sym->mark = true;

  if (sym->baseType && !symbol_IsLinearizable(sym->baseType)) {
    if (sym->cls != sc_symRef) 
      diag_printf("Symbol \"%s\" extends \"%s\"\n", 
		   symbol_QualifiedName(sym,'.'),
		   symbol_QualifiedName(sym->baseType, '.'));
    goto fail;
  }
    
  if (sym->type && 
      !symbol_IsReferenceType(sym->type) && 
      !symbol_IsLinearizable(sym->type)) {
    if (sym->cls != sc_symRef) 
      diag_printf("Symbol \"%s\" type is \"%s\"\n", 
		   symbol_QualifiedName(sym, '.'),
		   symbol_QualifiedName(sym->type, '.'));
    goto fail;
  }

  if (sym->value && !symbol_IsLinearizable(sym->value))
    goto fail;

  for(i = 0; i < vec_len(sym->children); i++) {
    if (!symbol_IsLinearizable(symvec_fetch(sym->children,i))) {
      if (sym->cls != sc_symRef) 
	diag_printf("Symbol \"%s\" contains \"%s\"\n", 
		     symbol_QualifiedName(sym, '.'),
		     symbol_QualifiedName(symvec_fetch(sym->children,i), '.'));
      goto fail;
    }
  }

  for (const auto eachRaised : sym->raised) {
    if (!symbol_IsLinearizable(eachRaised)) {
      if (sym->cls != sc_symRef) 
	diag_printf("Symbol \"%s\" contains \"%s\"\n", 
		     symbol_QualifiedName(sym, '.'),
		     symbol_QualifiedName(eachRaised, '.'));
      goto fail;
    }
  }

  sym->mark = false;

  return true;

 fail:
  sym->mark = false;

  return false;
}

void
symbol_ClearAllMarks(Symbol *sym)
{
  unsigned int i;
  sym->mark = false;

  if (sym->baseType)
    symbol_ClearAllMarks(sym->baseType);
  if (sym->type)
    symbol_ClearAllMarks(sym->type);
  if (sym->value && sym->cls != sc_symRef)
    symbol_ClearAllMarks(sym->value);

  for(i = 0; i < vec_len(sym->children); i++)
    symbol_ClearAllMarks(symvec_fetch(sym->children,i));

  for (const auto eachRaised : sym->raised)
    symbol_ClearAllMarks(eachRaised);
}

void 
symbol_ComputeDependencies(Symbol *sym, PtrVec *depVec)
{
  unsigned i;

  if (sym->cls == sc_symRef) {
    Symbol *targetUoc = symbol_UnitOfCompilation(sym->value);
    if (targetUoc != symbol_UnitOfCompilation(sym)) {
      if (!ptrvec_contains(depVec, targetUoc))
	ptrvec_append(depVec, targetUoc);
    }

    return;
  }

  if (sym->baseType)
    symbol_ComputeDependencies(sym->baseType, depVec);

  if (sym->type)
    symbol_ComputeDependencies(sym->type, depVec);

  if (sym->value)
    symbol_ComputeDependencies(sym->value, depVec);
    
  for(i = 0; i < vec_len(sym->children); i++)
    symbol_ComputeDependencies(symvec_fetch(sym->children,i), depVec);

  for (const auto eachRaised : sym->raised)
    symbol_ComputeDependencies(eachRaised, depVec);
}

void 
symbol_ComputeTransDependencies(Symbol *sym, PtrVec *depVec)
{
  unsigned i;

  if (sym->cls == sc_symRef) {
    Symbol *targetUoc = symbol_UnitOfCompilation(sym->value);
    if (!ptrvec_contains(depVec, targetUoc)) {
      ptrvec_append(depVec, targetUoc);
      symbol_ComputeTransDependencies(targetUoc, depVec);
    }

    return;
  }

  if (sym->baseType)
    symbol_ComputeTransDependencies(sym->baseType, depVec);

  if (sym->type)
    symbol_ComputeTransDependencies(sym->type, depVec);

  if (sym->value)
    symbol_ComputeTransDependencies(sym->value, depVec);
    
  for(i = 0; i < vec_len(sym->children); i++)
    symbol_ComputeTransDependencies(symvec_fetch(sym->children,i), depVec);

  for (const auto eachRaised : sym->raised)
    symbol_ComputeTransDependencies(eachRaised, depVec);
}

int
symbol_SortByName(const void *v1, const void *v2)
{
  Symbol *s1 = *((Symbol **)v1);
  Symbol *s2 = *((Symbol **)v2);

  /* two depths are the same */
  return strcmp(s1->name, s2->name);
}

int
symbol_SortByQualifiedName(const void *v1, const void *v2)
{
  Symbol *s1 = *((Symbol **)v1);
  Symbol *s2 = *((Symbol **)v2);

  /* two depths are the same */
  return strcmp(symbol_QualifiedName(s1,'_'), symbol_QualifiedName(s2,'_'));
}

bool
symbol_IsFixedSerializable(Symbol *sym)
{
  unsigned i;
  bool result = true;

  sym = symbol_ResolveType(sym);
  
  if (symbol_IsReferenceType(sym))
    return true;

  if (sym->type && ! symbol_IsFixedSerializable(sym->type))
    return false;

  switch(sym->cls) {
  case sc_primtype:
    {
      if (sym->v.lty == lt_string)
	return false;
      if (sym->v.lty == lt_integer && sym->v.i == 0)
	return false;
      break;
    }
  case sc_seqType:
  case sc_bufType:
    return false;

  case sc_enum:
    return true;

  default:
    break;
  }
  
  for(i = 0; i < vec_len(sym->children); i++) {
    Symbol *child = vec_fetch(sym->children, i);
    if (!symbol_IsTypeSymbol(child))
      result = result && symbol_IsFixedSerializable(child->type);
  }

  for (const auto eachRaised : sym->raised)
    result = result && symbol_IsFixedSerializable(eachRaised);

  return result;
}

bool
symbol_IsDirectSerializable(Symbol *sym)
{
  return symbol_IsFixedSerializable(sym);
}

extern MP_INT compute_value(Symbol *s);

unsigned
symbol_alignof(Symbol *s)
{
  switch(s->cls) {
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
	  return bits/8;
	  break;
	}
      case lt_string:
	{
	  unsigned bits = mpz_get_ui(s->v.i);
	  return bits/8;
	}
      case lt_void:
	assert(false);
	break;
      }
      break;
    }
  case sc_enum:
    {
      // FIX: I suspect strongly that this is wrong, but I am
      // following C convention here.
      return ENUMERAL_SIZE;
      break;
    }
  case sc_struct:
    {
      unsigned i;
      unsigned align = 0;

      /* Alignment of struct is alignment of worst member. */
      for(i = 0; i < vec_len(s->children); i++) {
	Symbol *child = vec_fetch(s->children, i);
	if (!symbol_IsTypeSymbol(child))
	  align = max(align, symbol_alignof(child->type));
      }
      return align;
    }
  case sc_seqType:
  case sc_bufType:
    {
      /* Alignment of the vector struct: */
      return TARGET_LONG_SIZE;
    }
  case sc_typedef:
  case sc_member:
  case sc_formal:
  case sc_outformal:
  case sc_arrayType:
    {
      return symbol_alignof(s->type);
    }

  case sc_union:
    {
      unsigned i;
      unsigned align = 0;

      /* Alignment of union is alignment of worst member. */
      for(i = 0; i < vec_len(s->children); i++) {
	Symbol *child = vec_fetch(s->children, i);
	align = max(align, symbol_alignof(child->type));
      }

      return align;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      return 0;
    }
  case sc_symRef:
    {
      return symbol_alignof(s->value);
    }
  default:
    {
      diag_fatal(1, "Alignment of unknown type (class %s) for symbol \"%s\"\n", 
		 symbol_ClassName(s),
		 symbol_QualifiedName(s, '.'));
      break;
    }
  }

  return 0;
}

#if 0
unsigned
symbol_sizeof(Symbol *s)
{
  unsigned len = 0;

  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty) {
      case lt_integer:
      case lt_unsigned:
      case lt_char:
      case lt_bool:
      case lt_float:
	{
	  unsigned bits = mpz_get_ui(s->v.i);
	  return bits/8;
	}
      case lt_string:
	diag_fatal(1, "Strings are not yet supported \"%s\"\n", 
		   symbol_QualifiedName(s, '.'));
	break;
      case lt_void:
	assert(false);
	break;
      }

      break;
    }
  case sc_enum:
    {
      // FIX: I suspect strongly that this is wrong, but I am
      // following C convention here.
      return ENUMERAL_SIZE;
    }
  case sc_struct:
    {
      unsigned i;

      for(i = 0; i < vec_len(s->children); i++) {
	Symbol *child = vec_fetch(s->children, i);
	if (!symbol_IsType(child)) {
	  len = round_up(len, symbol_alignof(child));
	  len += symbol_sizeof(child->type);
	}
      }

      /* Need to round up to next struct alignment boundary for array
	 computationions to be correct: */
      len = round_up(len, symbol_alignof(s));
      return len;
    }
  case sc_typedef:
  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      return symbol_sizeof(s->type);
    }
  case sc_seqType:
  case sc_bufType:
    {
      /* Vector structure itself is aligned at a 4-byte boundary,
	 contains a 4-byte max, a 4-byte length and a 4-byte pointer
	 to the members. Need to reserve space for that. */
      return TARGET_LONG_SIZE * 3;

#if 0
      offset = round_up(offset, TARGET_LONG_SIZE);
      offset += (2 * TARGET_LONG_SIZE);

      /* This is a bit tricky, as the size of the element type and the
	 alignment of the element subtype may conspire to yield a
	 "hole" at the end of each vector element. */
      MP_INT bound = compute_value(s->value);
      unsigned align = symbol_alignof(s->type);
      unsigned elemSize = 0;
      symbol_estimateSize(s->type, elemSize);
      elemSize = round_up(elemSize, align);

      mpz_t total;
      mpz_init(total);
      mpz_mul_ui(total, &bound, elemSize);
      offset += mpz_get_ui(total);
      break;
#endif
    }
  case sc_arrayType:
    {
      /* This is a bit tricky, as the size of the element type and the
	 alignment of the element subtype may conspire to yield a
	 "hole" at the end of each vector element. */
      MP_INT bound = compute_value(s->value);

      unsigned align = symbol_alignof(s->type);
      unsigned elemSize = symbol_sizeof(s->type);
      elemSize = round_up(elemSize, align);

      mpz_t total;
      mpz_init(total);
      mpz_mul_ui(total, &bound, elemSize);
      return mpz_get_ui(total);
      break;
    }
  case sc_union:
    {
      assert(false);
      break;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      break;
    }
  case sc_symRef:
    {
      return symbol_sizeof(s->value);
      break;
    }
  default:
    {
      diag_fatal(1, "Size computation of unknown type for symbol \"%s\"\n", 
		 symbol_QualifiedName(s, '.'));
      break;
    }
  }

  return 0;
}
#endif

unsigned
symbol_directSize(Symbol *s)
{
  unsigned len = 0;

  switch(s->cls) {
  case sc_primtype:
    {
      switch(s->v.lty) {
      case lt_integer:
      case lt_unsigned:
      case lt_char:
      case lt_bool:
      case lt_float:
	{
	  unsigned bits = mpz_get_ui(s->v.i);
	  return bits/8;
	}
      case lt_string:
	diag_fatal(1, "Strings are not yet supported \"%s\"\n", 
		   symbol_QualifiedName(s, '.'));
	break;
      case lt_void:
	assert(false);
	break;
      }

      break;
    }
  case sc_enum:
    {
      // FIX: I suspect strongly that this is wrong, but I am
      // following C convention here.
      return ENUMERAL_SIZE;
    }
  case sc_struct:
    {
      unsigned i;

      for(i = 0; i < vec_len(s->children); i++) {
	Symbol *child = vec_fetch(s->children, i);
	if (!symbol_IsTypeSymbol(child)) {
	  len = round_up(len, symbol_alignof(child));
	  len += symbol_directSize(child->type);
	}
      }

      /* Need to round up to next struct alignment boundary for array
	 computationions to be correct: */
      len = round_up(len, symbol_alignof(s));
      return len;
    }
  case sc_typedef:
  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      return symbol_directSize(s->type);
    }
  case sc_seqType:
  case sc_bufType:
    {
      /* Vector structure itself is aligned at a 4-byte boundary,
	 contains a 4-byte max, a 4-byte length and a 4-byte pointer
	 to the members. Need to reserve space for that. */
      return TARGET_LONG_SIZE * 3 ;
    }
  case sc_arrayType:
    {
      /* This is a bit tricky, as the size of the element type and the
	 alignment of the element subtype may conspire to yield a
	 "hole" at the end of each vector element. */
      MP_INT bound = compute_value(s->value);

      unsigned align = symbol_alignof(s->type);
      unsigned elemSize = symbol_directSize(s->type);
      elemSize = round_up(elemSize, align);

      {
	mpz_t total;
	mpz_init(total);
	mpz_mul_ui(total, &bound, elemSize);
	return mpz_get_ui(total);
      }
    }
  case sc_union:
    {
      assert(false);
      break;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      break;
    }
  case sc_symRef:
    {
      return symbol_directSize(s->value);
      break;
    }
  default:
    {
      diag_fatal(1, "Size computation of unknown type for symbol \"%s\"\n", 
		 symbol_QualifiedName(s, '.'));
      break;
    }
  }

  return 0;
}

unsigned
symbol_indirectSize(Symbol *s)
{
  unsigned len = 0;

  switch(s->cls) {
  case sc_primtype:
  case sc_enum:
    return 0;
  case sc_struct:
    {
      unsigned i;

      for(i = 0; i < vec_len(s->children); i++) {
	Symbol *child = vec_fetch(s->children, i);
	if (!symbol_IsTypeSymbol(child))
	  len += symbol_indirectSize(child->type);
      }

      return len;
    }
  case sc_typedef:
  case sc_member:
  case sc_formal:
  case sc_outformal:
    {
      return symbol_indirectSize(s->type);
    }
  case sc_seqType:
    {
      /* Not yet implemented */
      assert(false);
      break;
    }
  case sc_arrayType:
    {
      return 0;			/* for now */
    }

  case sc_bufType:
    {
      /* This is a bit tricky, as the size of the element type and the
	 alignment of the element subtype may conspire to yield a
	 "hole" at the end of each vector element.
      
	 Eventually, this will be further complicated by the need to
	 deal with indirect types containing indirect types. At the
	 moment it just doesn't deal with this case at all. */
      MP_INT bound = compute_value(s->value);

      unsigned align = symbol_alignof(s->type);
      unsigned elemSize = symbol_directSize(s->type);
      elemSize = round_up(elemSize, align);

      {
	mpz_t total;
	mpz_init(total);
	mpz_mul_ui(total, &bound, elemSize);
	return mpz_get_ui(total);
      }
    }
  case sc_union:
    {
      assert(false);
      break;
    }
  case sc_interface:
  case sc_absinterface:
    {
      assert(false);
      break;
    }
  case sc_symRef:
    {
      return symbol_indirectSize(s->value);
      break;
    }
  default:
    {
      diag_fatal(1, "Size computation of unknown type for symbol \"%s\"\n", 
		 symbol_QualifiedName(s, '.'));
      break;
    }
  }

  return 0;
}

