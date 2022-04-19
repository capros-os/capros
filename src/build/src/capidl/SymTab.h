/*
 * Copyright (C) 2002, The EROS Group, LLC.
 * Copyright (C) 2008, Strawberry Development Group.
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

#pragma once

#include <gmp.h>  // GNU multiple precision library
#include <applib/Intern.h>
#include <applib/PtrVec.h>
#include <vector>

#define ENUMERAL_SIZE    4
#define TARGET_LONG_SIZE 4

/*
 * Token type structure. Using a structure for this is a quite
 * amazingly bad idea, but using a union makes the C++ constructor
 * logic unhappy....
 */

enum LitType {			/* literal type */
  lt_void,
  lt_integer,
  lt_unsigned,
  lt_float,
  lt_char,
  lt_bool,
  lt_string,
};
typedef enum LitType LitType;

struct LitValue {
  mpz_t         i;		/* large precision integers */
  double        d;		/* doubles, floats */
  LitType	lty;
  /* no special field for lt_string, as the name is the literal */
};
typedef struct LitValue LitValue;

/* There is a design issue hiding here: how symbolic should the output
 * of the IDL compiler be? I think the correct answer is "very", in
 * which case we may need to do some tail chasing for constants. Thus,
 * I consider a computed constant value to be a symbol.
 */

enum SymClass {
#define SYMCLASS(x,n) sc_##x,
#include "symclass.def"
#undef  SYMCLASS
};
typedef enum SymClass SymClass;

#define SYMDEBUG

#define SF_NOSTUB     0x1u
#define SF_NO_OPCODE  0x2u

class Symbol {
public:
  // Constructor
  Symbol(const char *nm, bool isActiveUOC, SymClass sc);

  InternedString const name;
#ifdef SYMDEBUG
  InternedString qualifiedName;
#endif
  InternedString docComment = nullptr;  /* unknown type */

  bool           mark = false;		/* used for circularity detection */
  bool const     isActiveUOC;	/* created in an active unit of compilation */

  Symbol         *nameSpace = nullptr;	/* containing namespace */
  std::vector<Symbol*> children;	/* members of the scope */
  std::vector<Symbol*> raised;	/* exceptions raised by this method/interface */

  SymClass       cls;

  Symbol *       type = nullptr;		/* type of an identifier */
  Symbol *       baseType = nullptr;	/* base type, if extension */
  Symbol *       value = nullptr;		/* value of a constant */
				  
  bool           complete;	  /* used for top symbols only. */
  unsigned       ifDepth = 0;	/* depth of inheritance tree */
                              /* depth 0 arbitrarily reserved */

  unsigned       flags = 0;		/* flags that govern code generation */

  LitValue       v;		/* only for sc_value and sc_builtin */

  /* Using a left-recursive grammar is always really messy, because
   * you have to do the scope push/pop by hand, which is at best
   * messy. Bother.
   */

  // Public methods:

  InternedString QualifiedName(char sep);
};

#ifdef __cplusplus
extern "C" {
#endif

void symbol_InitSymtab();

void symbol_ClearAllMarks(Symbol *s);
Symbol *symbol_construct(const char *name, bool isActiveUOC, SymClass);
Symbol *symbol_FindPackageScope();

/* Creates a new symbol of the specified type in the currently
 * active scope. This is not a constructor because it must be able
 * to return failure in the event of a symbol name collision. */
Symbol *symbol_create_inScope(const char *nm, bool isActiveUOC, SymClass, Symbol *inScope);
Symbol *symbol_create(const char *nm, bool isActiveUOC, SymClass);
Symbol *symbol_createPackage(const char *nm, Symbol *inPkg);
Symbol *symbol_createRef(const char *nm, bool isActiveUOC);
Symbol *symbol_createRef_inScope(const char *nm, bool isActiveUOC, Symbol *inScope);

// Create a RaisesRef in scope symbol_curScope
Symbol *symbol_createRaisesRef(const char *nm, bool isActiveUOC);
Symbol *symbol_gensym(SymClass, bool isActiveUOC);
Symbol *symbol_gensym_inScope(SymClass, bool isActiveUOC, Symbol *inScope);
  
Symbol *symbol_MakeKeyword(const char *nm, SymClass sc,
			   LitType lt,
			   unsigned value);

Symbol *symbol_MakeIntLitFromMpz(const mpz_t);
Symbol *symbol_MakeIntLit(const char *);
Symbol *symbol_MakeStringLit(const char *);
Symbol *symbol_MakeCharLit(const char *);
Symbol *symbol_MakeFloatLit(const char *);
Symbol *symbol_MakeExprNode(const char *op, Symbol *left,
			     Symbol *right);

/* Following, when applied to scalar types (currently only integer
 * types) will produce min and max values. */
Symbol *symbol_MakeMaxLit(Symbol *);
Symbol *symbol_MakeMinLit(Symbol *);

Symbol *symbol_LookupIntType(unsigned bitsz, bool uIntType);

Symbol *symbol_LookupChild(Symbol *, const char *name, Symbol *bound);
Symbol *symbol_LexicalLookup(Symbol *, const char *name, Symbol *bound);

bool symbol_ResolveSymbolReference(Symbol *);

void symbol_PushScope(Symbol *newScope);
void symbol_PopScope();

#ifdef SYMDEBUG
void symbol_QualifyNames(Symbol *);
#else
inline void symbol_QualifyNames(Symbol *)
{
}
#endif
bool symbol_ResolveReferences(Symbol *);
void symbol_ResolveIfDepth(Symbol *);
bool symbol_TypeCheck(Symbol *);
bool symbol_IsLinearizable(Symbol *);
bool symbol_IsFixedSerializable(Symbol *);
bool symbol_IsDirectSerializable(Symbol *);

void symbol_ComputeDependencies(Symbol *, PtrVec *);
void symbol_ComputeTransDependencies(Symbol *, PtrVec *);

unsigned symbol_alignof(Symbol *);
unsigned symbol_directSize(Symbol *);
unsigned symbol_indirectSize(Symbol *);

Symbol *symbol_UnitOfCompilation(Symbol *);

/* Return TRUE iff the passed symbol is itself (directly) a type
   symbol. Note that if you update this list you should also update it
   in the switch in symbol_ResolveType() */
static inline bool symbol_IsBasicType(Symbol *sym)
{
  if (sym == NULL)
    return 0;
  
  return (sym->cls == sc_primtype ||
	  sym->cls == sc_enum ||
	  sym->cls == sc_struct ||
	  sym->cls == sc_union ||
	  sym->cls == sc_interface ||
	  sym->cls == sc_absinterface ||
	  sym->cls == sc_seqType ||
	  sym->cls == sc_bufType ||
	  sym->cls == sc_arrayType);
}

/* Chase down symrefs to find the actual defining reference of a symbol */
static inline Symbol *symbol_ResolveRef(Symbol *sym)
{
  while (sym->cls == sc_symRef)
    sym = sym->value;

  return sym;
}

/* Assuming that the passed symbol is a type symbol, chase through
   symrefs and typedefs to find the actual type. */
Symbol *symbol_ResolveType(Symbol *sym);

/* Return TRUE iff the symbol is a type symbol */
static inline bool symbol_IsTypeSymbol(Symbol *sym)
{
  sym = symbol_ResolveRef(sym);
  
  return symbol_IsBasicType(sym) || (sym->cls == sc_typedef);
}

/* Return TRUE iff the symbol is of the provided type */
static inline bool symbol_IsType(Symbol *sym, SymClass sc)
{
  Symbol *typeSym = symbol_ResolveType(sym);
  return (typeSym && (typeSym->cls == sc));
}

/* Return TRUE iff the symbol, after resolving name references, is a
   typedef. Certain parameter types are legal, but only if they appear
   in typedefed form. */
static inline bool symbol_IsTypedef(Symbol *sym)
{
  sym = symbol_ResolveRef(sym);
  return (sym->cls == sc_typedef);
}

#if 0
/* Return TRUE iff the type of this symbol is some sort of aggregate
   type. */
static inline bool symbol_IsAggregateType(Symbol *sym)
{
  sym = symbol_ResolveType(sym);

  return (sym->cls == sc_struct ||
	  sym->cls == sc_union ||
	  sym->cls == sc_bufType ||
	  sym->cls == sc_seqType ||
	  sym->cls == sc_arrayType);
}
#endif

static inline bool symbol_IsValidParamType(Symbol *sym)
{
  Symbol *typeSym = symbol_ResolveType(sym);
    
  /* Sequences, Buffers, are invalid, but typedefs of these are okay. */
  if (typeSym->cls == sc_seqType || typeSym->cls == sc_bufType)
    return symbol_IsTypedef(sym);

  return symbol_IsBasicType(typeSym);
}

static inline bool symbol_IsValidMemberType(Symbol *sym)
{
  sym = symbol_ResolveType(sym);

  /* Buffers and typdefs of buffers are disallowed. */
  if (sym->cls == sc_bufType)
    return false;

  return symbol_IsBasicType(sym);
}

static inline bool symbol_IsValidSequenceBaseType(Symbol *sym)
{
  sym = symbol_ResolveType(sym);

  /* Variable number of client preallocated buffers makes no sense. */
  if (sym->cls == sc_bufType)
    return false;

  /* Temporary restriction: we do not allow sequences of
     sequences. This is an implementation restriction, not a
     fundamental problem. */
  if (sym->cls == sc_arrayType || sym->cls == sc_seqType)
    return false;
  
  return symbol_IsBasicType(sym);
}

static inline bool symbol_IsValidBufferBaseType(Symbol *sym)
{
  sym = symbol_ResolveType(sym);

  /* Temporary restriction: we do not allow sequences of
     sequences. This is an implementation restriction, not a
     fundamental problem. */
  if (sym->cls == sc_arrayType || sym->cls == sc_seqType ||
      sym->cls == sc_bufType)
    return false;

  return symbol_IsBasicType(sym);
}

static inline bool symbol_IsVarSequenceType(Symbol *sym)
{
  return (sym->cls == sc_seqType || sym->cls == sc_bufType);
}

static inline bool symbol_IsFixSequenceType(Symbol *sym)
{
  return (sym->cls == sc_arrayType);
}

static inline bool symbol_IsConstantValue(Symbol *sym)
{
  sym = symbol_ResolveRef(sym);
  
  return (sym->cls == sc_const ||
	  sym->cls == sc_value ||
	  sym->cls == sc_arithop);
}

static inline bool symbol_IsInterface(Symbol *sym)
{
  sym = symbol_ResolveType(sym);

  return (sym->cls == sc_interface ||
	  sym->cls == sc_absinterface);
}

static inline bool symbol_IsOperation(Symbol *sym)
{
  sym = symbol_ResolveType(sym);

  return (sym->cls == sc_operation);
}

static inline bool symbol_IsException(Symbol *sym)
{
  sym = symbol_ResolveType(sym);

  return (sym->cls == sc_exception);
}

/* Return TRUE if the symbol is a type that is passed by reference 
   rather than by copy. */
static inline bool symbol_IsReferenceType(Symbol *sym)
{
  return symbol_IsInterface(sym);
}

static inline bool symbol_IsVoidType(Symbol *sym)
{
  sym = symbol_ResolveType(sym);
     
  if (sym->cls != sc_primtype)
    return false;

  return (sym->v.lty == lt_void);
}

static inline bool symbol_IsAnonymous(Symbol *sym)
{
  return (sym->name[0] == '#');
}

InternedString symname_join(const char *n1, const char *n2, char sep);

InternedString symbol_QualifiedName(Symbol *, char sep);

unsigned long long symbol_CodedName(Symbol *);

static inline const char *symbol_ClassName(Symbol *s)
{
  extern const char *symbol_sc_names[];
  return symbol_sc_names[s->cls];
}
static inline bool symbol_IsScope(Symbol *s)
{
  extern unsigned symbol_sc_isScope[];
  return symbol_sc_isScope[s->cls] ? true : false;
}

/* sorting helpers */
int
symbol_SortByName(const void *v1, const void *v2);

int
symbol_SortByQualifiedName(const void *v1, const void *v2);

#ifdef __cplusplus
}
#endif


extern Symbol *symbol_curScope;
extern Symbol *symbol_voidType;
extern Symbol *symbol_UniversalScope;
extern Symbol *symbol_KeywordScope;
extern Symbol *symbol_CurrentScope;

