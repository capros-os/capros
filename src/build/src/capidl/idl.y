%{
/*
 * Copyright (C) 2002, The EROS Group, LLC.
 * Copyright (C) 2007, Strawberry Development Group.
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

/*
 * This file contains an implementation of the CapIDL grammar.
 *
 * CapIDL was originally defined by Mark Miller, with much feedback
 * from the eros-arch list, especially Jonathan Shapiro. CapIDL is in
 * turn distantly derived from the Corba IDL, by the OMG.
 *
 * Further information on CapIDL can be found at http://www.capidl.org
 *
 * This grammar differs from the base CapIDL grammar as follows:
 *
 *   1. It is a subset. Various pieces of the CapIDL grammar are
 *      omitted until I can implement them and test them
 *      satisfactorily.
 *
 *   2. It uses fixed-precision integer arithmetic for constants. I
 *      believe that this deviation from CORBA was unjustified. Mark
 *      and I have yet to sort this out, and it is possible that this
 *      tool will switch to true integer arithmetic at some time in
 *      the future.
 *
 *   3. Tokens and productions are all properly typed.
 *
 *   4. It actually has an implementation. I have endeavoured to
 *      strongly separate the code generator from the parser, in order
 *      that the code generator can be easily modified by others. In
 *      particular, I have already reached the point where I have
 *      decided that there will be distinct code generators for C and
 *      for C++, if only to allow me to validate the merits of the
 *      proposed exception handling model.
 *
 *   5. RESERVED is a token, not a production. Catching that error in
 *      the grammar only makes things uglier.
 *
 *
 * This grammar probably requires Bison, as I think it's really really
 * stupid to have a parser that must be reinitialized in order to
 * process more than one file.
 *
 * Note that capidl (the tool) tries hard NOT to be a real
 * compiler. The purpose of this implementation is to translate
 * straightforward input into straightforward output. The one place
 * where this should not be true is in the client-side stub
 * generator. The current client stub generator is quite inefficient,
 * but I did not want to try making one that was more efficient when
 * the capability invocation specification for EROS itself is
 * presently up in the air.
 */

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/xmalloc.h>
#include <applib/App.h>
#include <applib/Intern.h>
#include "SymTab.h"
#include "ParseType.h"

#define MYLEXER ((MyLexer *)lexer)

extern bool showparse;	/* global debugging flag */

#define SHOWPARSE(s) if (showparse) diag_printf(s)
#define SHOWPARSE1(s,x) if (showparse) diag_printf(s,x)

#if 0
mpz_t next_exception_value;
 mpz_init(next_exception_value);
 mpz_set_ui(next_exception_value, 1);
#endif


int num_errors = 0;  /* hold the number of syntax errors encountered. */

#define YYSTYPE ParseType

#define YYPARSE_PARAM lexer
#define YYLEX_PARAM lexer

#define yyerror(s) mylexer_ReportParseError(lexer, s)

#include "Lexer.h"

void import_symbol(InternedString ident);
extern const char *basename(const char *);
extern int mylexer_lex (YYSTYPE *lvalp, MyLexer *);
#define yylex mylexer_lex
extern void output_symdump(Symbol *);
%}

%pure_parser

/* Categorical terminals */
%token <tok>        Identifier
%token <tok>        IntegerLiteral
%token <tok>        CharLiteral
%token <tok>        FloatingPtLiteral
%token <tok>        StringLiteral
 


/* Corba Keywords */
%token <NONE> BOOLEAN CASE CHAR CONST DEFAULT 
%token <NONE> DOUBLE ENUM EXCEPTION FLOAT 
%token <tok>  tTRUE tFALSE
%token <tok>  tMIN tMAX
%token <NONE> INTERFACE LONG MODULE OBJECT BYTE ONEWAY OUT RAISES
%token <NONE> SHORT STRING STRUCT SWITCH TYPEDEF
%token <NONE> UNSIGNED UNION VOID WCHAR WSTRING

/* Other Keywords */
%token <NONE> INTEGER REPR 

/* Reserved Corba Keywords */
%token <tok> ANY ATTRIBUTE CONTEXT FIXED IN INOUT NATIVE 
%token <tok> READONLY TRUNCATABLE VALUETYPE
%token <tok> SEQUENCE BUFFER ARRAY

%token <NONE>  CLIENT
%token <NONE>  NOSTUB
%type  <flags> opr_qual

/* Other Reserved Keywords. Note that BEGIN conflicts with flex usage
 * for input state transitions, so we cannot use that. */
%token <NONE> ABSTRACT AN AS tBEGIN BEHALF BIND 
%token <NONE> CATCH CLASS CONSTRUCTOR 
%token <NONE> DECLARE DEF DEFINE DEFMACRO DELEGATE DEPRECATED DISPATCH DO
%token <NONE> ELSE END ENSURE EVENTUAL ESCAPE EVENTUALLY EXPORT EXTENDS 
%token <NONE> FACET FINALLY FOR FORALL FUNCTION
%token <NONE> IMPLEMENTS IS
%token <NONE> LAMBDA LET LOOP MATCH META METHOD METHODS 
%token <NONE> NAMESPACE ON 
%token <NONE> PACKAGE PRIVATE PROTECTED PUBLIC 
%token <NONE> RELIANCE RELIANT RELIES RELY REVEAL
%token <NONE> SAKE SIGNED STATIC
%token <NONE> SUPPORTS SUSPECT SUSPECTS SYNCHRONIZED
%token <NONE> THIS THROWS TO TRANSIENT TRY 
%token <NONE> USES USING UTF8 UTF16 
%token <NONE> VIRTUAL VOLATILE WHEN WHILE

/* operators */
%token <NONE> OPSCOPE /* :: */

%type  <NONE> start
%type  <NONE> unit_of_compilation
%type  <NONE> top_definitions
%type  <NONE> top_definition
%type  <NONE> package_dcl
%type  <NONE> case_definitions
%type  <NONE> case_definition
%type  <sym>  if_extends
%type  <NONE> if_definitions
%type  <NONE> if_definition
%type  <tok>  name_def
%type  <NONE> namespace_dcl
%type  <NONE> namespace_members
%type  <NONE> struct_dcl
%type  <sym>  except_dcl
%type  <sym>  except_name_def
%type  <NONE> union_dcl
%type  <NONE> enum_dcl
%type  <NONE> typedef_dcl
%type  <NONE> const_dcl
%type  <NONE> element_dcl
%type  <NONE> interface_dcl
%type  <NONE> repr_dcl
%type  <sym>  type
%type  <sym>  param_type
%type  <sym>  const_expr
%type  <sym>  const_sum_expr
%type  <sym>  const_mul_expr
%type  <sym>  const_term
%type  <tok>  ident
%type  <tok>  scoped_name
%type  <tok>  dotted_name
%type  <sym>  scalar_type
%type  <sym>  seq_type
%type  <sym>  buf_type
%type  <sym>  array_type
%type  <sym>  string_type
%type  <sym>  integer_type
%type  <sym>  floating_pt_type
%type  <sym>  char_type
%type  <sym>  switch_type
%type  <NONE> member
%type  <NONE> member_list
%type  <NONE> case
%type  <NONE> cases
%type  <NONE> case_label
%type  <NONE> case_labels
%type  <NONE> enum_defs
%type  <sym>  literal
%type  <NONE> opr_dcl
%type  <sym>  ret_type
%type  <NONE> params
%type  <NONE> param_list
%type  <sym>  param
%type  <NONE> param_2s
%type  <NONE> param_2_list
%type  <NONE> raises
%type  <NONE> advice
%type  <tok>  reserved

/* Grammar follows */
%%

start: unit_of_compilation
   { SHOWPARSE("start -> unit_of_compilation\n"); }
 ;

unit_of_compilation: package_dcl /* uses_dcls */ top_definitions
  {

    SHOWPARSE("unit_of_compilation -> "
	      "package_dcl uses_dcls top_definitions\n");
    /* We appear to have imported this package successfully. Mark the
       import completed: */

    assert(symbol_curScope->cls == sc_package);

    /* Pop the package scope... */
    symbol_PopScope() ;
  };

package_dcl: PACKAGE dotted_name ';'
  {
    Symbol *sym;

    SHOWPARSE("PACKAGE dotted_name\n");

    sym = symbol_LookupChild(symbol_UniversalScope, $2.is, 0);

    if (sym == 0)
      sym = symbol_createPackage($2.is, symbol_UniversalScope);

    sym->v.lty = lt_bool;	/* true if import is completed. */
    mpz_set_ui(sym->v.i, 0);

    symbol_PushScope(sym);
  } ;

//uses_dcls:
//    /* empty */ { SHOWPARSE("uses_dcls -> <empty>\n"); }
//  | uses_dcls uses_dcl
//    { SHOWPARSE("uses_dcls -> uses_dcls uses_dcl\n"); }
//  ;
//
//uses_dcl: USES dotted_name ';'
//  {
//    SHOWPARSE("uses_dcl -> USES dotted_name\n");
//
//    /* First, see if the desired package is already imported: */
//    import_symbol($2.is);
//
//    Symbol *pkgSym = symbol_LookupChild(symbol_UniversalScope,$2.is);
//
//    Symbol *sym = symbol_create($2.is, sc_usepkg, 0);
//    sym->value = pkgSym;
//  }

top_definitions:
    /* empty */ { SHOWPARSE("top_definitions -> <empty>\n"); }
 | top_definitions top_definition
   { SHOWPARSE("top_definitions -> top_definitions top_definition\n");
   }
 ;

top_definition:
// All top-level definitions must introduce a namespace!
    interface_dcl ';'
    { SHOWPARSE("top_definition -> interface_dcl ';' \n"); }
  | struct_dcl ';'
    { SHOWPARSE("top_definition -> struct_dcl ';' \n"); }
  | except_dcl ';'
    { 
      SHOWPARSE("top_definition -> except_dcl ';' \n"); 
      diag_printf("%s:%d: %s -- top level exception definitions not permitted\n",
		   MYLEXER->current_file, 
		   MYLEXER->current_line,
		   (const char *) $1->name);
      num_errors++;
    }
  | union_dcl ';'
    { SHOWPARSE("top_definition -> union_dcl ';' \n"); }
  | enum_dcl ';'
    { SHOWPARSE("top_definition -> enum_dcl ';' \n"); }
  | namespace_dcl ';'
    { SHOWPARSE("top_definition -> namespace_dcl ';' \n"); }
  ;

other_definition:
    typedef_dcl ';'
    { SHOWPARSE("other_definition -> typedef_dcl ';' \n"); }
  | const_dcl ';'
    { SHOWPARSE("other_definition -> const_dcl ';' \n"); }
  | repr_dcl ';'
    { SHOWPARSE("other_definition -> repr_dcl ';' \n"); }
  ;

/********************** Names ***************************/



/**
 * A defining-occurrence of an identifier.  The identifier is defined
 * as a name within the scope in which it textually appears.
 */
name_def:
        ident
    { SHOWPARSE("name_def -> ident\n"); }
 ;

/**
 * A use-occurrence of a name.  The name may be unqualified, fully
 * qualified, or partially qualified.  Corba scoping rules are used to
 * associate a use-occurrence with a defining-occurrence.
 */
scoped_name:
        dotted_name {
	  SHOWPARSE("scoped_name -> dotted_name\n");

	  $$ = $1;
	}
//  |     OPSCOPE dotted_name {
//	  SHOWPARSE("scoped_name -> dotted_name\n");
//
//
//	  $$ = $2;
//	}
  ;

dotted_name:
        ident {                         /* unqualified */
	  SHOWPARSE("dotted_name -> ident\n");

	  $$ = $1;
	}
 |      dotted_name '.' ident {     /* qualified */
	  SHOWPARSE("dotted_name -> dotted_name '.' ident\n");

	  $$.is = intern_concat($1.is,
				intern_concat(intern("."), $3.is));
        }
 ;

/**
 * These extra productions exist so that a better diagnostic can be
 * given when a reserved keyword is used where a normal identifier is
 * expected.  The reserved: production should eventually list all the
 * reserved keywords.
 */
ident:
        Identifier {
          SHOWPARSE("ident -> Identifier\n");
          $$ = $1; 
        }
 |      reserved   {
	  SHOWPARSE("ident -> reserved\n");
	  diag_printf("%s:%d: %s is a reserved word\n",
		      MYLEXER->current_file, 
		       MYLEXER->current_line,
		      (const char *) $1.is);
	  num_errors++;
	  YYERROR;
	 }
 ;

/* I'm lazy -- not bothering with all the reserved SHOWPARSE calls */
reserved:
        ANY | ATTRIBUTE | CONTEXT | FIXED | IN | INOUT | NATIVE 
 |      READONLY | SEQUENCE | TRUNCATABLE | VALUETYPE 
 ;



/********************** Types ***************************/



/**
 * Can be a capability type, a pure data type, or a mixed type.  Most
 * of the contexts where a mixed type is syntactically accepted
 * actually require a pure data type, but this is enforced after
 * parsing.
 */
type:
	param_type
   { SHOWPARSE("type -> param_type\n"); $$ = $1;}
 |      seq_type
   { SHOWPARSE("type -> seq_type\n"); $$ = $1; }
 |      buf_type
   { SHOWPARSE("type -> buf_type\n"); $$ = $1; }
 ;

/* Sequence types cannot be paramater types, because some output 
 * languages require a struct declaration for them, and this in turn
 * requires that a type name exist for purposes of determining
 * compatibility of argument passing.
 */

param_type:
        scalar_type                     /* atomic pure data value */
   { SHOWPARSE("param_type -> scalar_type\n"); $$ = $1; }
 |      string_type                     /* sequences of characters */
   { SHOWPARSE("param_type -> string_type\n"); $$ = $1; }
 |      array_type
   { SHOWPARSE("param_type -> array_type\n"); $$ = $1; }
 |      OBJECT                          /* generic capability */ {
          SHOWPARSE("param_type -> OBJECT\n");
	  $$ = 0;
          /* FIX: Need generic interface type */
	}
 |      scoped_name {                   /* must name a defined type */
          Symbol *symRef;

          SHOWPARSE("param_type -> scoped_name\n");

	  symRef = symbol_createRef($1.is, MYLEXER->isActiveUOC);

	  $$ = symRef;
	}
 ;

scalar_type:
        integer_type {                  /* subranges of INTEGER */
          SHOWPARSE("scalar_type -> integer_type\n");
          $$ = $1;
        }
 |      floating_pt_type {              /* various IEEE precisions */
          SHOWPARSE("scalar_type -> floating_pt_type\n");
          $$ = $1;
        }
 |      char_type {                     /* subranges of Unicode WCHAR */
          SHOWPARSE("scalar_type -> char_type\n");
          $$ = $1;
        }
 |      BOOLEAN {                       /* TRUE or FALSE */
          SHOWPARSE("scalar_type -> BOOLEAN\n");
	  $$ = symbol_LookupChild(symbol_KeywordScope, "#bool", 0);
	}
 ;

/**
 * The only values of these types are integers.  The individual types
 * differ regarding what subrange of integer they accept.  Not all
 * syntactically expressible variations will supported in the
 * forseable future.  We expect to initially support only INTEGER<N>
 * and UNSIGNED<N> for N == 16, 32, 64, as well as UNSIGNED<8>.
 */
/* SHAP: (1) ranges are a content qualifier, not a type
         (2) ranges temporarily ommitted -- grammar needs revision to
             get them right.
*/
integer_type:
        INTEGER                         /* == INTEGER<8> */ {
          SHOWPARSE("integer_type -> INTEGER\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#int", 0);
        }
 |      INTEGER '<' const_expr '>'      /* [-2**(N-1),2**(N-1)-1] */ {
          SHOWPARSE("integer_type -> INTEGER '<' const_expr '>'\n");
	  switch (mpz_get_ui($3->v.i)) {
	  case 8:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#int8", 0);
	    break;
	  case 16:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#int16", 0);
	    break;
	  case 32:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#int32", 0);
	    break;
	  case 64:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#int63", 0);
	    break;
	  default:
	    {
	      diag_printf("%s:%d: syntax error -- bad integer size\n",
			   MYLEXER->current_file, 
			   MYLEXER->current_line);
	      num_errors++;
	      YYERROR;
	      break;
	    }
	  }
        }
 |      BYTE                            /* == INTEGER<8> */ {
          SHOWPARSE("integer_type -> BYTE\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#int8", 0);
        }
 |      SHORT                           /* == INTEGER<16> */ {
          SHOWPARSE("integer_type -> SHORT\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#int16", 0);
        }
 |      LONG                            /* == INTEGER<32> */ {
          SHOWPARSE("integer_type -> LONG\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#int32", 0);
        }
 |      LONG LONG                       /* == INTEGER<64> */ {
          SHOWPARSE("integer_type -> LONG LONG\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#int64", 0);
        }

 |      UNSIGNED '<' const_expr '>'     /* [0,2**N-1] */ {
          SHOWPARSE("integer_type -> UNSIGNED '<' const_expr '>'\n");
	  switch (mpz_get_ui($3->v.i)) {
	  case 8:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#uint8", 0);
	    break;
	  case 16:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#uint16", 0);
	    break;
	  case 32:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#uint32", 0);
	    break;
	  case 64:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#uint64", 0);
	    break;
	  default:
	    {
	      diag_printf("%s:%d: syntax error -- bad integer size\n",
			   MYLEXER->current_file, 
			   MYLEXER->current_line);
	      num_errors++;
	      YYERROR;
	      break;
	    }
	  }
        }
 |      UNSIGNED BYTE                   /* == UNSIGNED<8> */ {
          SHOWPARSE("integer_type -> UNSIGNED BYTE\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#uint8", 0);
        }
 |      UNSIGNED SHORT                  /* == UNSIGNED<16> */ {
          SHOWPARSE("integer_type -> UNSIGNED SHORT\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#uint16", 0);
        }
 |      UNSIGNED LONG                   /* == UNSIGNED<32> */ {
          SHOWPARSE("integer_type -> UNSIGNED LONG\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#uint32", 0);
        }
 |      UNSIGNED LONG LONG              /* == UNSIGNED<64> */ {
          SHOWPARSE("integer_type -> UNSIGNED LONG LONG\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#uint64", 0);
        }
 ;

/**
 * The only values of these types are real numbers, positive and
 * negative infinity, and the NaNs defined by IEEE.  As each IEEE
 * precision is a unique beast, the sizes may only be those defined as
 * standard IEEE precisions.  We expect to initially support only
 * FLOAT<32> and FLOAT<64>.
 */
floating_pt_type:
        FLOAT '<' const_expr '>'      /* IEEE std floating precision N */ {
          SHOWPARSE("floating_pt_type -> FLOAT '<' const_expr '>'\n");
	  switch (mpz_get_ui($3->v.i)) {
	  case 32:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#float32", 0);
	    break;
	  case 64:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#float64", 0);
	    break;
	  case 128:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#float128", 0);
	    break;
	  default:
	    {
	      diag_printf("%s:%d: syntax error -- unknown float size\n",
			   MYLEXER->current_file, 
			   MYLEXER->current_line);
	      num_errors++;
	      YYERROR;
	      break;
	    }
	  }
	}
 |      FLOAT                         /* == FLOAT<32> */ {
          SHOWPARSE("floating_pt_type -> FLOAT\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#float32", 0);
	}
 |      DOUBLE                        /* == FLOAT<64> */ {
          SHOWPARSE("floating_pt_type -> DOUBLE\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#float64", 0);
	}
 |      LONG DOUBLE                   /* == FLOAT<128> */ {
          SHOWPARSE("floating_pt_type -> LONG DOUBLE\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#float128", 0);
	}
 ;

/**
 * The only values of these types are 32 bit Unicode characters.
 * These types differ regarding the subrange of Unicode character
 * codes they will accept.  We expect to initially support only
 * WCHAR<7> (ascii), WCHAR<8> (latin-1), WCHAR<16> (java-unicode), and
 * WCHAR<32> (full unicode). 
 */
/* SHAP: (1) Ascii is a subrange of char, not a distinct type.
         (2) ranges should work on all wchar variants.
	 (3) ranges are a content qualifier, not a type
         (4) ranges temporarily ommitted -- grammar needs revision to
             get them right.
*/
char_type:
        WCHAR '<' const_expr '>'        /* WCHAR<0,2**N-1> */ {
          SHOWPARSE("char_type -> WCHAR '<' const_expr '>'\n");
	  switch (mpz_get_ui($3->v.i)) {
	  case 8:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#wchar8", 0);
	    break;
	  case 16:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#wchar16", 0);
	    break;
	  case 32:
	    $$ = symbol_LookupChild(symbol_KeywordScope,"#wchar32", 0);
	    break;
	  default:
	    {
	      diag_printf("%s:%d: syntax error -- bad wchar size\n",
			   MYLEXER->current_file, 
			   MYLEXER->current_line);
	      num_errors++;
	      YYERROR;
	      break;
	    }
	  }
	}
 |      CHAR             /* WCHAR<8> */ {
          SHOWPARSE("char_type -> CHAR\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#wchar8", 0);
        } 
 |      WCHAR            /* WCHAR<32> == Unicode character */ {
          SHOWPARSE("char_type -> WCHAR\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#wchar32", 0);
        } 
 ;

/**
 * A sequence is some number of repeatitions of some base type.  The
 * number of repetitions may be bounded or unbounded.  Strings are
 * simply sequences of characters, but are singled out as special for
 * three reasons: 1) The subrange of character they repeat does not
 * include '\0'. 2) The marshalled representation includes an extra
 * '\0' character after the end of the string. 3) Many languages
 * have a special String data type to which this must be bound.
 */

seq_type:
        SEQUENCE '<' type '>' {	// some number of repetitions of type
	  Symbol *seqSym = symbol_gensym_inScope(sc_seqType, MYLEXER->isActiveUOC, 0);
	  seqSym->type = $3;
	  $$ = seqSym;
	}
 |      SEQUENCE '<' type ',' const_expr '>' { // no more than N repetitions of type
	  Symbol *seqSym = symbol_gensym_inScope(sc_seqType, MYLEXER->isActiveUOC, 0);
	  seqSym->type = $3;
	  seqSym->value = $5;
	  $$ = seqSym;
        }
 ;

buf_type:
        BUFFER '<' type '>' {	// some number of repetitions of type
	  Symbol *seqSym = symbol_gensym_inScope(sc_bufType, MYLEXER->isActiveUOC, 0);
	  seqSym->type = $3;
	  $$ = seqSym;
	}
 |      BUFFER '<' type ',' const_expr '>' { // no more than N repetitions of type
	  Symbol *seqSym = symbol_gensym_inScope(sc_bufType, MYLEXER->isActiveUOC, 0);
	  seqSym->type = $3;
	  seqSym->value = $5;
	  $$ = seqSym;
        }
 ;

array_type:
        ARRAY '<' type ',' const_expr '>' { // exactly N repetitions of type
	  Symbol *seqSym = symbol_gensym_inScope(sc_arrayType, MYLEXER->isActiveUOC, 0);
	  seqSym->type = $3;
	  seqSym->value = $5;
	  $$ = seqSym;
	}
 ;

/**
   Contrary to MarkM's original design assumption (above), strings are 
   NOT simply sequence types with a reserved terminator. The reason is
   that they can usually be bound to a distinguished abstract
   source-language type. This implies that the type of STRING is not
   necessarily the same as the type sequence<char> (array of
   characters).

   A curious result of this is that unbounded sequence types probably
   should not be accepted as parameter or return types, because it
   isn't at all simple to generate the appropriate wrapper structures
   on the fly to make the parameter passing work out.
*/
string_type:
        STRING {
          $$ = symbol_LookupChild(symbol_KeywordScope,"#wstring8", 0);
        }
// |      STRING '<' const_expr '>' {     // == WCHAR<8>[N]
//	  Symbol *seqSym = symbol_gensym(sc_seqType, 0);
//	  seqSym->type = symbol_LookupChild(symbol_KeywordScope,"#wchar8");
//	  seqSym->value = $3;
//	  $$ = seqSym;
//        }
 |      WSTRING {                       // == WCHAR<32>[]
          $$ = symbol_LookupChild(symbol_KeywordScope,"#wstring32", 0);
        }
// |      WSTRING '<' const_expr '>' {    // == WCHAR<32>[N]
//	  Symbol *seqSym = symbol_gensym(sc_seqType, 0);
//	  seqSym->type = symbol_LookupChild(symbol_KeywordScope,"#wchar32");
//	  seqSym->value = $3;
//	  $$ = seqSym;
//        }
 ;


typedef_dcl:
        TYPEDEF type name_def {
	  Symbol *sym;
          SHOWPARSE("typedef_dcl -> TYPEDEF type name_def\n");
	  sym = symbol_create($3.is, MYLEXER->isActiveUOC, sc_typedef);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- typedef identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $3.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $2;
	  sym->docComment = mylexer_grab_doc_comment(lexer);
	}
 ;


namespace_dcl:
        NAMESPACE name_def '{'  {
	  Symbol *sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_scope);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- namespace identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Struct name is a scope for its formals: */
	  symbol_PushScope(sym);
	}
        namespace_members '}'  {
          SHOWPARSE("namespace_dcl -> NAMESPACE name_def '{' namespace_members '}'\n");
	  symbol_PopScope();
	}
 ;

namespace_members:
	/* empty */ { }
 |      namespace_members top_definition { }
 |      namespace_members other_definition { }
 ;

/********************** Structs ***************************/



/**
 * Like a C struct, a struct defines an aggregate type consisting of
 * each of the member types in order.  Whereas the members of a
 * sequence are accessed by numeric index, the members of a structure
 * are accessed by member name (ie, field name). <p>
 *
 * Like a Corba or CapIDL module, a CapIDL struct also defines a named
 * scope, such that top_definitions between the curly brackets define
 * names in that scope.
 */
struct_dcl:
        STRUCT name_def '{'  {
	  Symbol *sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_struct);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- struct identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Struct name is a scope for its formals: */
	  symbol_PushScope(sym);
	}
        member_list '}'  {
          SHOWPARSE("struct_dcl -> STRUCT name_def '{' member_list '}'\n");
	  symbol_PopScope();
	}
 ;

member_list:
        member_list member
   { SHOWPARSE("member_list -> member_list member\n"); }
 |      member
   { SHOWPARSE("member_list -> member\n"); }
 ;

/*SHAP:        top_definition         /* defines a name inside this scope */
member:
        struct_dcl ';'
   { SHOWPARSE("member -> struct_dcl ';'\n"); }
 |      union_dcl  ';'
   { SHOWPARSE("member -> union_dcl ';'\n"); }
 |      enum_dcl ';'
   { SHOWPARSE("member -> enum_dcl ';'\n"); }
 | namespace_dcl ';'
    { SHOWPARSE("member -> namespace_dcl ';' \n"); }
 |      const_dcl ';'
   { SHOWPARSE("member -> const_dcl ';'\n"); }
 |      repr_dcl ';'
   { SHOWPARSE("member -> repr_dcl ';'\n"); }
 |      element_dcl ';'
   { SHOWPARSE("member -> element_dcl ';'\n"); }
 ;



/********************** Exceptions ***************************/


/**
 * Structs that are sent (as in RAISES) to explain problems
 */
except_dcl:
        except_name_def '{' {
	  /* Struct name is a scope for its formals: */
	  symbol_PushScope($1);
        }
        member_list '}' {
          SHOWPARSE("except_dcl -> except_name_def '{' member_list '}'\n");
	  symbol_PopScope();
          $$ = $1;
	}
|      except_name_def /* '{' '}' */ { 
          SHOWPARSE("except_dcl -> except_name_def '{' '}'\n");
          $$ = $1;
        }
 ;


except_name_def:
        EXCEPTION name_def '=' const_expr {
	  Symbol *sym;
          SHOWPARSE("except_name_def -> EXCEPTION name_def '=' const_expr\n");
	  sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_exception);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- exception identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = $4;
	  sym->type = symbol_LookupChild(symbol_KeywordScope,"#int", 0);
	  sym->docComment = mylexer_grab_doc_comment(lexer);

          $$ = sym;
        }
 |      EXCEPTION name_def {
	  Symbol *sym;
          SHOWPARSE("except_name_def -> EXCEPTION name_def\n");
	  sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_exception);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- exception identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = symbol_MakeIntLit("0");
	  sym->type = symbol_voidType;
	  sym->docComment = mylexer_grab_doc_comment(lexer);

          $$ = sym;
        }
;
/********************** Discriminated Unions ***************************/


/**
 * Like the Corba discriminated union, this has a typed scalar that is
 * compared against the case labels to determine what the rest of the
 * data is.  So this is an aggregate data type consisting of the value
 * to be switched on followed by the element determined by this
 * value.  Unlike the Corba union, we also name the field holding the
 * value switched on. 
 *
 * The union as a whole creates a named nested scope for further name
 * definitions (as does module, struct, and interface), but the
 * individual case labels do not create further subscopes.
 */
union_dcl:
        UNION name_def '{' {
	  Symbol *sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_union);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- union identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Struct name is a scope for its formals: */
	  symbol_PushScope(sym);
	}
        SWITCH '(' switch_type name_def ')' '{'  {
	  Symbol *sym = symbol_create($8.is, MYLEXER->isActiveUOC, sc_switch);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- switch identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $8.is);
	    num_errors++;
	    YYERROR;
	  }

	  sym->type = $7;
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Struct name is a scope for its formals: */
	  /* symbol_PushScope(sym); */
	}
        cases 
            '}' ';'
        '}' {
          SHOWPARSE("union_dcl -> UNION name_def '{' SWITCH '(' switch_type name_def ')' '{' cases '}'\n");
	  /* symbol_PopScope(); */
	  symbol_PopScope();
	}
 ;

/**
 * One may only switch non scalar types other than floating point.
 * Any objections?  We expect to initially support only small enough
 * subranges of these types that an array lookup implementation is
 * reasonable. Let's say 0..255.
 */
switch_type:
        integer_type            /* subranges of INTEGER */ {
          SHOWPARSE("switch_type -> integer_type\n");
	  $$ = $1;
	}
 |      char_type               /* subranges of Unicode WCHAR */ {
          SHOWPARSE("switch_type -> char_type\n");
	  $$ = $1;
	}
 |      BOOLEAN                 /* TRUE or FALSE */ {
          SHOWPARSE("switch_type -> BOOLEAN\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,"#bool", 0);
        }
 |      scoped_name {           /* must name one of the other switch_types */
          SHOWPARSE("switch_type -> scoped_name\n");

	  $$ = symbol_createRef($1.is, MYLEXER->isActiveUOC);
	}
 ;

cases:
        { 
	  Symbol *sym = symbol_gensym(sc_caseScope, MYLEXER->isActiveUOC);
	  symbol_PushScope(sym);
	} case {
          SHOWPARSE("cases -> case\n");
          symbol_PopScope();
	}
 |      cases case {
          SHOWPARSE("cases -> cases case\n");
        }
 ;

/**
 * Each case consists of one or more case labels, zero or more name
 * definitions (scoped to the union as a whole), and one element
 * declaration.  (Note: I would like to scope these definitions to
 * the case, but the case has no natural name.)
 */
case:
        case_labels case_definitions element_dcl ';' {
          SHOWPARSE("case -> case_labels case_definitions element_dcl\n");
        }
 ;

case_definitions:
        /* empty */ {
          SHOWPARSE("case_definitions -> <empty>\n");
   }
 | case_definitions case_definition {
          SHOWPARSE("case_definitions -> case_definitions case_definition\n");

	  diag_printf("%s:%d: syntax error -- declarations not"
		       " permitted within switch\n",
		       MYLEXER->current_file, 
		       MYLEXER->current_line);
	  num_errors++;
	  YYERROR;
   }
 ;

case_definition:
        struct_dcl ';'
   { SHOWPARSE("case_definition -> struct_dcl ';'\n"); }
 |      union_dcl  ';'
   { SHOWPARSE("case_definition -> union_dcl ';'\n"); }
 |      namespace_dcl ';'
    { SHOWPARSE("case_definition -> namespace_dcl ';' \n"); }
 |      enum_dcl ';'
   { SHOWPARSE("case_definition -> enum_dcl ';'\n"); }
 |      const_dcl ';'
   { SHOWPARSE("case_definition -> const_dcl ';'\n"); }
 |      repr_dcl ';'
   { SHOWPARSE("case_definition -> repr_dcl ';'\n"); }
 ;

case_labels:
        case_label
   { SHOWPARSE("case_labels -> case_label\n"); }
 |      case_labels case_label
   { SHOWPARSE("case_labels -> case_labels case_label\n"); }
 ;

case_label:
        CASE const_expr ':' {
	  char *s;
	  Symbol *sym;
          SHOWPARSE("case_label -> CASE const_expr ':'\n");

	  s = mpz_get_str(NULL, 10, $2->v.i);
	  sym = symbol_create(s, MYLEXER->isActiveUOC, sc_caseTag);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- case identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 s);
	    num_errors++;
	    YYERROR;
	  }
	  free(s);
	}
 |      DEFAULT ':' {
	  Symbol *sym;
          SHOWPARSE("case_label -> DEFAULT ':'\n");
	  sym = symbol_create("#default:", MYLEXER->isActiveUOC, sc_caseTag);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- default case reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line);
	    num_errors++;
	    YYERROR;
	  }
	}
 ;

element_dcl:
        type name_def {
          Symbol *sym;
          SHOWPARSE("element_dcl -> type name_def\n");
	  sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_member);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- element identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $1;
	  sym->docComment = mylexer_grab_doc_comment(lexer);
	}
 ;



/********************** Enums ***************************/


/**
 * Just as characters have character codes but the character is not
 * its character code (it is only represented by its character code),
 * so an enumerated type consists of a set of named enumerated values,
 * each of which happens to be represented by a unique integer.  This
 * declaration declares the name of the type and the names of the
 * values of that type.  <p>
 *
 * Are the value names scoped to the type name?  In order words, given
 * "enum Color { RED, GREEN, BLUE }", must one then say "Color::RED"
 * or simply "RED"?  What's Corba's answer? 
 */
/**
 * 
 */
enum_dcl:
        integer_type ENUM name_def '{' {
	  Symbol *sym = symbol_create($3.is, MYLEXER->isActiveUOC, sc_enum);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- enum identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $3.is);
	    num_errors++;
	    YYERROR;
	  }
          sym->type = $1;
	  sym->docComment = mylexer_grab_doc_comment(lexer);
	  mpz_set_ui(sym->v.i, 0); /* value for next enum member held here */
	  /* Enum name is a scope for its formals: */
	  symbol_PushScope(sym);
	}
        enum_defs '}' {
          SHOWPARSE("enum_dcl -> ENUM name_def '{' enum_defs '}'\n");
	  symbol_PopScope();
	}
//  |     ENUM '{' {
//	  Symbol *sym = symbol_gensym(sc_enum); /* anonymous */
//	  sym->docComment = mylexer_grab_doc_comment(lexer);
//	  mpz_set_ui(sym->v.i, 0); /* value for next enum member held here */
//	  /* Enum name is a scope for its formals: */
//	  symbol_PushScope(sym);
//	}
//        enum_defs '}' {
//          SHOWPARSE("enum_dcl -> ENUM /* anonymous */ '{' enum_defs '}'\n");
//	  symbol_PopScope();
//	}
 ;

enum_defs:
        name_def {
	  Symbol *theEnumTag;
	  Symbol *sym;

          SHOWPARSE("enum_defs -> name_def\n");
	  theEnumTag = symbol_curScope;
	  sym = symbol_create($1.is, MYLEXER->isActiveUOC, sc_const);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- econst identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $1.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = symbol_MakeIntLitFromMpz(theEnumTag->v.i);
	  sym->type = symbol_LookupChild(symbol_KeywordScope,"#int32", 0);
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Bump the enumeration's "next enumeral" value */
	  mpz_add_ui(theEnumTag->v.i, sym->value->v.i, 1);
	}
 |      name_def '=' const_expr {
          Symbol *theEnumTag;
	  Symbol *sym;
          SHOWPARSE("enum_defs -> name_def '=' const_expr\n");
	  theEnumTag = symbol_curScope;
	  sym = symbol_create($1.is, MYLEXER->isActiveUOC, sc_const);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- econst identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $1.is);
	    num_errors++;
	    YYERROR;
	  }
          sym->value = $3;
	  sym->type = symbol_LookupChild(symbol_KeywordScope,"#int32", 0);
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Bump the enumeration's "next enumeral" value */
	  mpz_add_ui(theEnumTag->v.i, sym->value->v.i, 1);
	}
 |      enum_defs ',' name_def {
          Symbol *theEnumTag;
	  Symbol *sym;
          SHOWPARSE("enum_defs -> enum_defs ',' name_def\n");
	  theEnumTag = symbol_curScope;
	  sym = symbol_create($3.is, MYLEXER->isActiveUOC, sc_const);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- econst identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $3.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = symbol_MakeIntLitFromMpz(theEnumTag->v.i);
	  sym->type = symbol_LookupChild(symbol_KeywordScope,"#int32", 0);
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Bump the enumeration's "next enumeral" value */
	  mpz_add_ui(theEnumTag->v.i, sym->value->v.i, 1);
        }
 |      enum_defs ',' name_def '=' const_expr {
          Symbol *theEnumTag;
	  Symbol *sym;
          SHOWPARSE("enum_defs -> enum_defs ',' name_def '=' const_expr\n");
	  theEnumTag = symbol_curScope;
	  sym = symbol_create($3.is, MYLEXER->isActiveUOC, sc_const);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- econst identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $3.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = $5;
	  sym->type = symbol_LookupChild(symbol_KeywordScope,"#int32", 0);
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Bump the enumeration's "next enumeral" value */
	  mpz_add_ui(theEnumTag->v.i, sym->value->v.i, 1);
        }
 ;



/********************** Constant Declarations ***************************/
const_dcl:
        CONST type name_def '=' const_expr {
	  Symbol *sym;
          SHOWPARSE("const_dcl -> CONST type name_def\n");
	  sym = symbol_create($3.is, MYLEXER->isActiveUOC, sc_const);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- constant identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $3.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->value = $5;
	  sym->type = $2;
	  sym->docComment = mylexer_grab_doc_comment(lexer);
	}
 ;

/********************** Constant Expressions ***************************/


/**
 * Eventually, we expect to support the full set of operators that
 * Corba constant exressions support, although we will interpret these
 * operators in a precision unlimited fashion.  (It is not appropriate
 * for a language neutral framework to use a standard for arithmetic
 * other than mathematics, unless motivated by efficiency.  Since
 * constant expressions are evaluated at CapIDL compile time,
 * efficiency isn't an issue.)
 */
const_expr: const_sum_expr {
          SHOWPARSE("const_expr -> const_sum_expr\n");
	  $$ = $1;
	}
 ;

const_sum_expr:
        const_mul_expr {
          SHOWPARSE("const_expr -> const_mul_expr\n");
	  $$ = $1;
        }
  |     const_mul_expr '+' const_mul_expr {
          SHOWPARSE("const_expr -> const_mul_expr '*' const_mul_expr\n");

	  $$ = symbol_MakeExprNode("+", $1, $3);
        }
  |     const_mul_expr '-' const_mul_expr {
          SHOWPARSE("const_expr -> const_mul_expr '/' const_mul_expr\n");

	  $$ = symbol_MakeExprNode("-", $1, $3);
        }
  ;

const_mul_expr:
        const_term {
          SHOWPARSE("const_mul_expr -> const_term\n");

          $$ = $1;
        }
  |     const_term '*' const_term {
          SHOWPARSE("const_expr -> const_mul_expr '*' const_mul_expr\n");

	  $$ = symbol_MakeExprNode("*", $1, $3);
        }
  |     const_term '/' const_term {
          SHOWPARSE("const_expr -> const_mul_expr '/' const_mul_expr\n");

	  $$ = symbol_MakeExprNode("/", $1, $3);
        }
  ;

const_term:
        scoped_name {
          SHOWPARSE("const_term -> scoped_name\n");

	  $$ = symbol_createRef($1.is, MYLEXER->isActiveUOC);
	}
|       literal {
          SHOWPARSE("const_term -> literal\n");
	  $$ = $1;
        }
 |      '(' const_expr ')' {
          SHOWPARSE("const_term -> '(' const_expr ')'\n");
          $$ = $2;
        }
 ;

literal:
        IntegerLiteral     { 
          SHOWPARSE("literal -> IntegerLiteral\n");
          $$ = symbol_MakeIntLit($1.is); 
        }
 |      '-' IntegerLiteral     { 
	  Symbol *left;
	  Symbol *right;
          SHOWPARSE("literal -> '-' IntegerLiteral\n");

	  left = symbol_MakeIntLit("0");
	  right = symbol_MakeIntLit($2.is);
	  $$ = symbol_MakeExprNode("-", left, right);
        }
 |      StringLiteral      { 
          SHOWPARSE("literal -> StringLiteral\n");
          $$ = symbol_MakeStringLit($1.is); 
        }
 |      CharLiteral        { 
          SHOWPARSE("literal -> CharLiteral\n");
          $$ = symbol_MakeCharLit($1.is); 
        }
 |      FloatingPtLiteral  { 
          SHOWPARSE("literal -> FloatingPtLiteral\n");
          $$ = symbol_MakeFloatLit($1.is); 
        }
 |      tMIN '(' integer_type ')' {
          Symbol *sym;
          SHOWPARSE("literal -> tMin '(' integer_type ')'\n");
	  sym = symbol_MakeMinLit($3);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- infinite integral types have no min/max\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line);
	    num_errors++;
	    YYERROR;
	  }
	  $$ = sym;
        }
 |      tMAX '(' integer_type ')' {
          Symbol *sym;
          SHOWPARSE("literal -> tMax '(' integer_type ')'\n");
          sym = symbol_MakeMaxLit($3);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- infinite integral types have no min/max\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line);
	    num_errors++;
	    YYERROR;
	  }
	  $$ = sym;
        }
 |      tTRUE  { 
          SHOWPARSE("literal -> tTRUE\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,$1.is, 0); 
        }
 |      tFALSE {
          SHOWPARSE("literal -> tFALSE\n");
          $$ = symbol_LookupChild(symbol_KeywordScope,$1.is, 0); 
        }
 ;



/******************* Interfaces / Capabilities **********************/


/* SHAP: In CORBA, and interface is a scope. While it defines
 * operations, it can also define structures, constants, and so
 * forth. That is, all top-level declarables except for interfaces and
 * modules can be declared within an interface scope. Interfaces and
 * modules are distinguished in that the notion of "interface" in the
 * CORBA IDL is "top level" (ignoring modules, which are optional),
 * and so interfaces cannot contain interfaces.
 *
 * To implement this, I revised MarkM's original grammar to replace
 * his <message> nonterminal with an <twoway_dcl> (which is what the
 * CORBA 2.4.1 grammar called operation declarations) and
 * re-introduced the original style of <interface> nonterminal with
 * <if_definitions> and <if_definition> as intermediate nonterminals
 * in place of <messages>. In some ways this is regrettable, as it
 * renders the "what is a capability" question less clear -- an
 * interface can now contain stuff in addition to the operations.
 */

/**
 * Defines a capability type.  The type of a capability is defined by
 * what you can send it.  By far the most common convention is to send
 * it a message, which consists of an order code, a sequence of
 * capability arguments, and a sequence of pure data arguments.  (For
 * present purposes, it would be inappropriate to try to support
 * an argument type that mixed data and capabilities.)  One of the
 * capability arguments is also special -- the resume argument.  When
 * the invoker of a capability does an EROS CALL, the resume argument
 * position is filled in by the OS with a Resume key which, when
 * invoked, will cause the caller to continue with the arguments to
 * the Resume key invocation. <p>
 *
 * CapIDL supports three levels of description, from most convenient,
 * conventional, and high level, to most flexible and low level. <p>
 *
 * The lowest level description is the "struct level", in which a
 * capability is defined as a one argument procedure with no return,
 * where the argument describes the capabilities and data that may be
 * passed to that capability.  At this level, there are not
 * necessarily any resume parameters.  However, when CALLing such a
 * capability, the OS generated Resume key will be passed in the first
 * capability parameter position.  At this level, there are not
 * necessarily any order codes.  However, the type will often by a
 * discriminated union switching on an enum, in which case the values
 * of this enum are the moral equivalent of order codes.  The struct
 * level is the only context in CapIDL where structs and unions can mix
 * data and capabilities. <p>
 *
 * Next is the "oneway message level" or just the "oneway level".
 * Here, a capability is described by explicitly declaring the
 * separate messages you can send to the capability.  This level
 * expands to the struct level by turning each message name into an
 * enum value (of an enum type specific to this capability type),
 * gathering all the arguments for each message in order into a
 * struct, and gathering all these enum values and argument structs
 * into one big discriminated union.  At this level, order codes are
 * implicit and built in, but are still not necessarily any resume
 * parameters. <p>
 *
 * Next is the "twoway message level" or just the "twoway level".
 * Here, a capability is still described as a set of messages, but any
 * one of these messages may instead be declared as twoway.  At the
 * twoway level, the resume parameter is implicit and built in as
 * well.  Instead, OUT parameters and a list of RAISEd Exceptions is
 * added to the oneway message declaration.  These effectively declare
 * the type of the Resume parameter, at the price of imposing some
 * conventional restrictions.  Specifically, the Resume parameter type
 * may only have one success order code, and all other order codes
 * must either be well known system exception codes, or must pass only
 * an Exception (which will be one of those in the RAISES clause).
 * The twoway level expands into the oneway level by turning these
 * extra declarations into an explicit type on an explicit Resume
 * parameter. 
 */
interface_dcl:
        INTERFACE name_def if_extends {
	  Symbol *sym;
#if 0
	  diag_printf("Attempting create of interface %s\n", $2.is);
#endif
	  sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_interface);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- interface identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }

	  if ($3)
	    sym->baseType = $3;

	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Procedure name is a scope for its formals: */
	  symbol_PushScope(sym);
        }
        raises '{' if_definitions '}'  { // message-levels
          SHOWPARSE("interface_dcl -> INTERFACE name_def if_extends raises '{' if_definitions '}'\n");
	  symbol_PopScope();
        }
 |      ABSTRACT INTERFACE name_def if_extends {
          Symbol *sym;
#if 0
	  diag_printf("Attempting create of interface %s\n", $3.is);
#endif
	  sym = symbol_create($3.is, MYLEXER->isActiveUOC, sc_absinterface);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- interface identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $3.is);
	    sym = symbol_LookupChild(symbol_curScope, $3.is, 0); 
	    diag_printf("First defined as %s \"%s\"\n",
			 symbol_ClassName(sym),
			 symbol_QualifiedName(sym,'_'));
	    num_errors++;
	    YYERROR;
	  }

	  if ($4)
	    sym->baseType = $4;

	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  /* Procedure name is a scope for its formals: */
	  symbol_PushScope(sym);
        }
        raises '{' if_definitions '}'  { // message-levels
          SHOWPARSE("interface_dcl -> ABSTRACT INTERFACE name_def if_extends raises '{' if_definitions '}'\n");
	  symbol_PopScope();
        }
 ;

if_extends: 
    /* empty */ {
      SHOWPARSE("if_extends -> <empty>\n");
      $$ = 0; 
    }
  | EXTENDS dotted_name {
      SHOWPARSE("if_extends -> EXTENDS dotted_name\n");

      $$ = symbol_createRef($2.is, MYLEXER->isActiveUOC);
    }
 ;

if_definitions:
    /* empty */ { }
 |      if_definitions if_definition {
          SHOWPARSE("if_definitions -> if_definitions if_definition\n");
        }
 ;

if_definition:
        struct_dcl ';'
   { SHOWPARSE("if_definition -> struct_dcl ';'\n"); }
 |      except_dcl ';'
   { SHOWPARSE("if_definition -> except_dcl ';'\n"); }
 |      union_dcl  ';'
   { SHOWPARSE("if_definition -> union_dcl ';'\n"); }
 |      namespace_dcl  ';'
   { SHOWPARSE("if_definition -> namespace_dcl ';'\n"); }
 |      enum_dcl ';'
   { SHOWPARSE("if_definition -> enum_dcl ';'\n"); }
 |      typedef_dcl ';'
   { SHOWPARSE("if_definition -> typedef_dcl ';'\n"); }
 |      const_dcl ';'
   { SHOWPARSE("if_definition -> const_dcl ';'\n"); }
 |      opr_dcl ';'
   { SHOWPARSE("if_definition -> opr_dcl ';'\n"); }
 |      repr_dcl ';'
   { SHOWPARSE("if_definition -> repr_dcl ';'\n"); }
 ;

/**
 * The semantics here is tricky, because the function symbol MUST be
 * created before any of the parameter symbols. This is necessary
 * because the function symbol forms a scope into which the parameters
 * must be installed. */
/* It is a nuisance that we have to enumerate the opr_qual
 * and non-oper_qual cases, but failing to do so causes a shift-reduce
 * conflict with enum declarations. */
opr_dcl:
        opr_qual ONEWAY VOID name_def '(' {
	  Symbol *sym = symbol_create($4.is, MYLEXER->isActiveUOC, sc_oneway);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- operation identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $4.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->flags = $1;
	  sym->type = symbol_voidType;
	  sym->docComment = mylexer_grab_doc_comment(lexer);
	  /* Procedure name is a scope for its formals: */
	  symbol_PushScope(sym);
	}
        params ')' {
          SHOWPARSE("opr -> ONEWAY VOID name_def '(' params ')'\n");
	  symbol_PopScope();
	}
 |      ONEWAY VOID name_def '(' {
	  Symbol *sym = symbol_create($3.is, MYLEXER->isActiveUOC, sc_oneway);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- operation identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $3.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = symbol_voidType;
	  sym->docComment = mylexer_grab_doc_comment(lexer);
	  /* Procedure name is a scope for its formals: */
	  symbol_PushScope(sym);
	}
        params ')' {
          SHOWPARSE("opr -> ONEWAY VOID name_def '(' params ')'\n");
	  symbol_PopScope();
	}
 |      opr_qual ret_type name_def '(' {
	  Symbol *sym = symbol_create($3.is, MYLEXER->isActiveUOC, sc_operation);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- operation identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $3.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->flags = $1;
	  sym->type = $2;
	  sym->docComment = mylexer_grab_doc_comment(lexer);
	  /* Procedure name is a scope for its formals: */
	  symbol_PushScope(sym);
	}
	param_2s ')' { 
        } raises {
	  symbol_PopScope();
          SHOWPARSE("opr -> ret_type name_def '(' param_2s ')' raises\n");
	}
 |      ret_type name_def '(' {
	  Symbol *sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_operation);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- operation identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $1;
	  sym->docComment = mylexer_grab_doc_comment(lexer);
	  /* Procedure name is a scope for its formals: */
	  symbol_PushScope(sym);
	}
	param_2s ')' { 
        } raises {
	  symbol_PopScope();
          SHOWPARSE("opr -> ret_type name_def '(' param_2s ')' raises\n");
	}
 ;

opr_qual:
        NOSTUB {
          SHOWPARSE("nostub -> NOSTUB\n");
          $$ = SF_NOSTUB;
        }
 |      CLIENT {
          SHOWPARSE("nostub -> CLIENT\n");
          $$ = SF_NO_OPCODE|SF_NOSTUB;
        }
 ;
       
/**
 * In expanding to the struct level, all the params are gathered into
 * a struct.  First come all the capability arguments, and then all
 * the pure data parameters.  A param cannot be of mixed data and
 * capability type.
 */
params:
        /*empty*/ {
          SHOWPARSE("params -> <empty>\n");
        }
 |      param_list {
          SHOWPARSE("params -> param_list\n");
        }
 ;

param_list:
        param {
          SHOWPARSE("param_list -> param\n");
        }
 |      param_list ',' param {
          SHOWPARSE("param_list -> param_list ',' param\n");
	}
 ;

/**
 * The parameter name is defined within the scope of this message
 * name.  In this one regard, each individual named message is also a
 * named nested scope.  This allows, for example, a following REPR
 * clause to give placement advice on a parameter by refering to it as
 * "messageName::parameterName" 
 *
 * In a discussion about hash generation, MarkM and I decided at one
 * point to make the formal parameter name optional, because it is not
 * semantically significant. The problem with making it optional is
 * that it breaks doc comments that refer to parameters by name.
 */
param:
        param_type name_def {
          Symbol *sym;
          SHOWPARSE("param -> type name_def\n");

	  sym = symbol_create($2.is, MYLEXER->isActiveUOC, sc_formal);
	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- parameter identifier \"%s\" reused\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line,
			 $2.is);
	    num_errors++;
	    YYERROR;
	  }
	  sym->type = $1;
	  sym->docComment = mylexer_grab_doc_comment(lexer);

	  $$ = sym;
	}
 ;

/**
 * In expanding to the oneway level, a non-VOID return type becomes a
 * first OUT argument of the same type, and the message is left with a
 * VOID return type.  The generated out parameter will be named
 * "_result", and so REPR advice can refer to the result by this name
 * even when the return type syntax is used.
 */
ret_type:
        type {
          SHOWPARSE("ret_type -> type\n");
	  $$ = $1;
        }
 |      VOID {
          SHOWPARSE("ret_type -> VOID\n");
	  $$ = symbol_voidType;
	}
 ;

param_2s:
        /*empty*/ {
          SHOWPARSE("param_2s -> <empty>\n");
        }
 |      param_2_list {
          SHOWPARSE("param_2s -> param_2_list\n");
        }
 ;

param_2_list:
        param_2 {
          SHOWPARSE("param_2_list -> param_2\n");
        }
 |      param_2_list ',' param_2 {
          SHOWPARSE("param_2_list -> param_2_list ',' param_2\n");
        }
 ;

/**
 * In expanding to the oneway level, a normal (IN) parameter is left
 * alone, but the out parameters are gathered together to form the
 * normal parameter list of the oneway success message to the Resume
 * parameter. 
 */
param_2:
        param {
          SHOWPARSE("param_2 -> param\n");
        }
 |      OUT param {
          SHOWPARSE("param_2 -> OUT param\n");
	  $2->cls = sc_outformal;
	}
 ;

/**
 * In expanding to the oneway level, each Exception listed in the
 * RAISES clause becomes a separate oneway problem reporting message
 * on the Resume parameter type, whose argument is just that
 * exception.  Should the order codes for these problem messages come
 * from the Exception declaration or the RAISES clause?  The first
 * would seem to make more sense, except there's no natural way to
 * coordinate uniqueness.  How do EROS or KeyKOS currently assign
 * order codes for reporting problems?
 */
raises:
 /*empty*/ {
          SHOWPARSE("raises -> <empty>\n");
        }
 |      RAISES '(' exceptions ')' {
          SHOWPARSE("raises -> RAISES '(' exceptions ')'\n");
        }
 ;

/* SHAP: Make the exceptions clause require at least one name, as the
 * entire clause can be eliminated if no exceptions are to be raised.
 */
exceptions:
        scoped_name {
          Symbol *sym;
          SHOWPARSE("exceptions -> scoped_name\n");

	  sym = symbol_createRaisesRef($1.is, MYLEXER->isActiveUOC);

	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- exception already raised\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line);
	    num_errors++;
	    YYERROR;
	  }
        }
 |      exceptions ',' scoped_name {
          Symbol *sym;
          SHOWPARSE("exceptions -> exceptions ',' scoped_name\n");

	  sym = symbol_createRaisesRef($3.is, MYLEXER->isActiveUOC);

	  if (sym == 0) {
	    diag_printf("%s:%d: syntax error -- exception already raise\n",
			 MYLEXER->current_file, 
			 MYLEXER->current_line);
	    num_errors++;
	    YYERROR;
	  }
        }
 ;




/********************** Representation ***************************/


/**
 * A placeholder for representation advice, including placement
 * advice.  Because, by Corba scoping rules, names in subscopes can be
 * refered to using paths, REPR advice could appear anywhere in the
 * compilation unit (ie, source file) after the declaration of the
 * thing being advised.  (Is there a possible security problem with
 * this?)  However, good style is for the REPR advice to follow as
 * closely as possible the declarations it is advising. <p>
 *
 * The reason the rest of the spec was careful never to refer to bit
 * representation, but rather speaks in terms of subranges (except for
 * floating point, where it's unavoidable), is that the rest of the
 * spec is only about semantics, not representation.  The REPR advice can
 * therefore be only about representation, not semantics. <p>
 *
 * Conflicting advice, or advice that specifies a representation not
 * able to preserve the semantics (such as insufficient bits for a
 * given subrange) must be caught and reported statically, and must
 * cause a failure to compile. <p>
 *
 * In the absence of advice, default advice applies.  This is
 * appropriate for human written source, but is a poor way for
 * programs to speak to other programs.  Instead, there needs to be a
 * tool for turning source capidl files into fully-advised capidl
 * files.  The fully advised files are likely to be canonicalized in
 * other ways as well.  These will be written once and read many
 * times, so the goal is to make it easier on the reading program.
 * The getAllegedType query may even return a fast binary equivalent
 * to a fully advised and somewhat canonicalized capIDL file.
 */
repr_dcl:
        REPR '{' advisories '}' {
          SHOWPARSE("repr_dcl -> REPR '{' advisories '}'\n");
        }
 |      REPR advice {
          SHOWPARSE("repr_dcl -> REPR advice\n");
        }
 ;

advisories:
        /*empty*/ {
          SHOWPARSE("advisories -> <empty>\n");
        }
 |      advisories advice {
          SHOWPARSE("advisories -> advisories advice\n");
        }
 ;

/**
 * The side to the right of the colon says how the thing named on the
 * left side should be represented, and where it should be placed.
 * This production is currently a placeholder until we figure out what
 * kind of advice we'd like to express.  As we figure this out, expect
 * the right side to grow. <p><pre>
 *
 * Some plausible meanings:
 *      enum_value: integer     // defines this enum value to be this integer
 *      message_name: integer   // gives the message this order code
 *      a_wstring: "UTF-8"      // represents the wide string in UTF-8
 *      a_wstring: "UTF-16"     // represents the wide string in UTF-16
 *      a_module: "LITTLE_ENDIAN" // inherited unless overridden?
 * </pre>
 */
advice:
        scoped_name ':' const_expr ';' {
          SHOWPARSE("advice -> scoped_name ':' const_expr ';'\n");
	}
 ;


%%

