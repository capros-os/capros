/*
This file is hereby placed in the public domain
*/

/*
  A YACC Grammar for CapIDL

  By Mark Miller, with much feedback from the eros-arch list,
  especially Jonathan Shapiro.

  Distantly derived from Corba IDL, by the OMG.
*/

/* CHANGE LOG:
 *
 * 9/18/2001:
 *
 * Made typedef_dcl a separate production for ease of semantics insert
 * (parallel to all the other cases)
 *
 * 9/17/2001:
 *
 * The biggest change in this round is the distinction between what
 * can be declared at global scope and what at interface
 * scope. Definitions at top level can now include interfaces,
 * const_dcls. Definitions within a module can no longer define
 * recursive modules. Look for the comment below containing "interface
 * is a scope"
 *
 * Support for integer subranges removed temporarily. There is an open
 * issue here: are subranges part of the type or are they a
 * description of valid values (i.e. a content qualifier)? In some
 * languages, they can only be implemented as the latter. Supporting
 * them correctly makes a mess of lots of things, so I killed them for
 * now.
 *
 * ASCII is now understood as a subrange of char, and is no longer a
 * distinct type. This implies that all char encodings are UTF-?
 * encodings, and if what you want is 8-bit extended ascii you are
 * shipping bytes.
 *
 * Simultaneously removed unbounded UNSIGNED, as languages that can
 * represent such values do so using unbounded signed integers.
 *
 * Empty top-level removed per CORBA spec
 *
 * Introduced MIN(integer_type) and MAX(integer_type) to yield
 * lower/upper bound for a given type, respectively, so as to make
 * things more immune to typedef changes.
 *
 * Typecases require at least one element -- this was a bug in the
 * CORBA grammar
 *
 * Everything now semicolon terminated, as per CORBA IDL
 *
 * OPSCOPE token name upcased, as it's not a categorical terminal
 *
 * Various token names from the original grammar have been prefixed
 * by 't' (e.g. TRUE => tTRUE) to avoid collision with common C
 * preprocessor symbols or (F)LEX convention.
 */

/* Categorical terminals */
%token Identifier
%token IntegerLiteral
%token CharLiteral
%token FloatingPtLiteral
%token StringLiteral



/* Corba Keywords */
%token BOOLEAN
%token CASE
%token CHAR
%token CONST
%token DEFAULT 
%token DOUBLE
%token ENUM
%token EXCEPTION
%token tFALSE
%token FLOAT 
%token INTERFACE
%token LONG
%token tMAX
%token tMIN
%token MODULE
%token OBJECT
%token BYTE
%token ONEWAY
%token OUT
%token RAISES
%token SHORT
%token STRING
%token STRUCT
%token SWITCH
%token tTRUE
%token TYPEDEF
%token UNSIGNED
%token UNION
%token VOID
%token WCHAR
%token WSTRING

/* Other Keywords */
%token INTEGER
%token REPR 

/* Reserved Corba Keywords */
%token ANY
%token ATTRIBUTE
%token CONTEXT
%token FIXED
%token IN
%token INOUT
%token NATIVE 
%token READONLY
%token SEQUENCE
%token TRUNCATABLE
%token VALUETYPE

/* Other Reserved Keywords. */
%token ABSTRACT
%token AN
%token AS
%token tBEGIN
%token BEHALF
%token BIND 
%token CATCH
%token CLASS
%token CONSTRUCTOR 
%token DECLARE
%token DEF
%token DEFINE
%token DEFMACRO
%token DELEGATE
%token DEPRECATED
%token DISPATCH
%token DO
%token ELSE
%token END
%token ENSURE
%token ESCAPE
%token EVENTUAL
%token EVENTUALLY
%token EXPORT
%token EXTENDS 
%token FACET
%token FINALLY
%token FOR
%token FORALL
%token FUNCTION
%token IMPLEMENTS
%token IN
%token IS
%token LAMBDA
%token LET
%token LOOP
%token MATCH
%token META
%token METHOD
%token METHODS 
%token NAMESPACE
%token ON 
%token PACKAGE
%token PRIVATE
%token PROTECTED
%token PUBLIC 
%token RELIANCE
%token RELIANT
%token RELIES
%token RELY
%token REVEAL
%token SAKE
%token SIGNED
%token STATIC
%token SUPPORTS
%token SUSPECT
%token SUSPECTS
%token SYNCHRONIZED
%token THIS
%token THROWS
%token TO
%token TRANSIENT
%token TRY 
%token USES
%token USING
%token UTF8
%token UTF16 
%token VIRTUAL
%token VOLATILE
%token WHEN
%token WHILE

/* operators */
%token OPSCOPE /* :: */
%token RESUME

/* Grammar follows */
%%

start: top_definition
 ;

/**
 * Defined a name to have a specified meaning in a scope.  The name
 * may be defined as a type, a scope, or a constant value.
 */
/* SHAP: Trailing semicolons added per CORBA IDL spec.
 * Also, does repr_dcl make sense at global scope, or should it be
 * part of the interface declaration? I suppose it's needed for struct
 * level, which is yet another line of questions (I'm really full of
 * questions).
 *
 * SHAP: empty top_definition removed per CORBA 2.4.1
 */
top_definition:
        MODULE name_def '{' mod_definitions '}' ';' // a scope
 |      interface_dcl ';'                       // a capability
 |      struct_dcl ';'                          // a struct
 |      except_dcl ';'                          // a struct to throw
 |      union_dcl  ';'                          // a discriminated union
 |      enum_dcl ';'                            // a set of const unsigneds
 |      typedef_dcl ';'                         // names a type
 |      TYPEDEF name_def ';'                    // forward declaration
 |      const_dcl ';'                           // constant value
 |      repr_dcl ';'                            // advises on representation
 ;

mod_definitions:
 /* empty */
 |      mod_definitions mod_definition
 ;

mod_definition:
        interface_dcl ';'                       // a capability
 |      struct_dcl ';'                          // a struct
 |      except_dcl ';'                          // a struct to throw
 |      union_dcl  ';'                          // a discriminated union
 |      enum_dcl ';'                            // a set of const unsigneds
 |      TYPEDEF type name_def ';'               // names a type
 |      TYPEDEF name_def ';'                    // forward declaration
 |      const_dcl ';'                           // constant value
 |      repr_dcl ';'                            // advises on representation
 ;
	
/********************** Names ***************************/



/**
 * A defining-occurrence of an identifier.  The identifier is defined
 * as a name within the scope in which it textually appears.
 */
name_def:
        ident
 ;

/**
 * A use-occurrence of a name.  The name may be unqualified, fully
 * qualified, or partially qualified.  Corba scoping rules are used to
 * associate a use-occurrence with a defining-occurrence.
 */
scoped_name:
        ident                           // unqualified
 |      OPSCOPE ident                   // global?
 |      scoped_name OPSCOPE ident       // qualified
          /* NOTE that yacc does not deal well with left recursion,
	   * and that in consequence this production may cause a
	   * shift/reduce error, however, the parser's error
	   * resolution (reduce) is correct. */
 ;

/**
 * These extra productions exist so that a better diagnostic can be
 * given when a reserved keyword is used where a normal identifier is
 * expected.  The reserved: production should eventually list all the
 * reserved keywords.
 */
ident:
        Identifier
 |      reserved
 ;
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
        scalar_type                     // atomic pure data value
 |      seq_type                        // sequences of other data types
 |      OBJECT                          // generic capability
 |      scoped_name                     // must name a defined type
 ;

scalar_type:
        integer_type                    // subranges of INTEGER
 |      floating_pt_type                // various IEEE precisions
 |      char_type                       // subranges of Unicode WCHAR
 |      BOOLEAN                         // TRUE or FALSE
 ;

/**
 * The only values of these types are integers.  The individual types
 * differ regarding what subrange of integer they accept.  Not all
 * syntactically expressible variations will supported in the
 * forseable future.  We expect to initially support only INTEGER<N>
 * and UNSIGNED<N> for N == 8, 16, 32, 64.
 */
/* SHAP: (1) ranges are a content qualifier, not a type
         (2) ranges temporarily ommitted -- grammar needs revision to
             get them right.
         (3) drop unbounded UNSIGNED type, since languages that can
	     represent them can and do represent them using unbounded
	     signed integers
*/
integer_type:
        INTEGER                         // all integers
 |      INTEGER '<' const_expr '>'      // [-2**(N-1),2**(N-1)-1]
 |      BYTE                            // == INTEGER<8>
 |      SHORT                           // == INTEGER<16>
 |      LONG                            // == INTEGER<32>
 |      LONG LONG                       // == INTEGER<64>
 |      UNSIGNED '<' const_expr '>'     // [0,2**N-1]
 |      UNSIGNED BYTE                   // == UNSIGNED<8>
 |      UNSIGNED SHORT                  // == UNSIGNED<16>
 |      UNSIGNED LONG                   // == UNSIGNED<32>
 |      UNSIGNED LONG LONG              // == UNSIGNED<64>
 ;

/**
 * The only values of these types are real numbers, positive and
 * negative infinity, and the NaNs defined by IEEE.  As each IEEE
 * precision is a unique beast, the sizes may only be those defined as
 * standard IEEE precisions.  We expect to initially support only
 * FLOAT<32> and FLOAT<64>.
 */
floating_pt_type:
        FLOAT '<' const_expr '>'      // IEEE std floating precision N
 |      FLOAT                         // == FLOAT<32>
 |      DOUBLE                        // == FLOAT<64>
 |      LONG DOUBLE                   // == FLOAT<128>
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
        WCHAR '<' const_expr '>'        // WCHAR<0,2**N-1>
 |      CHAR                            // WCHAR<8>
 |      WCHAR                           // WCHAR<32> == Unicode character
 ;

/**
 * A sequence is some number of repeatitions of some base type.  The
 * number of repeatitions may be bounded or unbounded.  Strings are
 * simply sequences of characters, but are singled out as special for
 * three reasons: 1) The subrange of character they repeat does not
 * include '\0'. 2) The marshalled representation includes an extra
 * '\0' character on after the end of the string. 3) Many languages
 * have a special String data type to which this must be bound.
 */
seq_type:
        type '[' ']'                    // some number of repeatitions of type
 |      type '[' const_expr ']'         // no more than N repeatitions of type
 |      STRING                          // == WCHAR<1,255>[]
 |      STRING '<' const_expr '>'       // == WCHAR<1,255>[N]
 |      WSTRING                         // == WCHAR<1,2**32-1>[]
 |      WSTRING '<' const_expr '>'      // == WCHAR<1,2**32-1>[N]
 ;


typedef_dcl:
        TYPEDEF type name_def
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
        STRUCT name_def '{' member_list '}'
 ;

member_list:
        member_list member
 |      member
 ;

members:
 /*empty*/
 |      members member
 ;

/*SHAP:        top_definition         /* defines a name inside this scope */
member:
        struct_dcl ';'                          // a struct
 |      union_dcl  ';'                          // a discriminated union
 |      enum_dcl ';'                            // a set of const unsigneds
 |      TYPEDEF type name_def ';'               // names a type
 |      TYPEDEF name_def ';'                    // forward declaration
 |      const_dcl ';'                           // constant value
 |      repr_dcl ';'                            // advises on representation
 |      type name_def ';'  /* defines an actual member (ie, field) */
 ;



/********************** Exceptions ***************************/


/**
 * Structs that are sent (as in RAISES) to explain problems
 */
except_dcl:
        EXCEPTION name_def '=' const_expr '{' members '}'
 |      EXCEPTION name_def '{' members '}'
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
 * top_definitions (as does module, struct, and interface), but the
 * individual case labels do not create further subscopes.
 */
union_dcl:
        UNION name_def '{' 
            SWITCH '(' switch_type name_def ')' '{' 
                cases 
            '}' ';'
        '}'
 ;

/**
 * One may only switch non scalar types other than floating point.
 * Any objections?  We expect to initially support only small enough
 * subranges of these types that an array lookup implementation is
 * reasonable. Let's say 0..255.
 */
switch_type:
        integer_type            // subranges of INTEGER
 |      char_type               // subranges of Unicode WCHAR
 |      BOOLEAN                 // TRUE or FALSE
 |      scoped_name             // must name one of the other switch_types
 ;

cases:
        case
 |      cases case
 ;

/**
 * Each case consists of one or more case labels, zero or more name
 * top_definitions (scoped to the union as a whole), and one element
 * declaration.  (Note: I would like to scope these top_definitions to
 * the case, but the case has no natural name.)
 */
/** SHAP: I think that allowing top_definitions inside other
 * top_definitions is in general a bad idea, because the scoping rules
 * need to be honored in the output language and there are too many
 * possible scoping rule resolutions.
 */
case:
        case_labels case_definitions element_dcl
 ;

case_definitions:
        /* empty */
 | case_definitions case_definition
 ;

case_definition:
        struct_dcl ';'                          // a struct
 |      union_dcl  ';'                          // a discriminated union
 |      enum_dcl ';'                            // a set of const unsigneds
 |      TYPEDEF type name_def ';'               // names a type
 |      TYPEDEF name_def ';'                    // forward declaration
 |      const_dcl ';'                           // constant value
 |      repr_dcl ';'                            // advises on representation

case_labels:
        case_label
 |      case_labels case_label
 ;

case_label:
        CASE const_expr ':'
 |      DEFAULT ':'
 ;

element_dcl:
        type name_def ';'
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
        ENUM name_def '{' enum_defs '}'
 ;

enum_defs:
        name_def
 |      enum_defs ',' name_def
 |      enum_defs ',' name_def '=' const_expr
 ;



/********************** Constant Declarations ***************************/
const_dcl:
        CONST type name_def '=' const_expr
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
const_expr:
        scoped_name
 |      literal
 |      '(' const_expr ')'
 ;

literal:
        IntegerLiteral
 |      StringLiteral
 |      CharLiteral
 |      FloatingPtLiteral
 |      tMIN '(' integer_type ')'
 |      tMAX '(' integer_type ')'
 |      tTRUE
 |      tFALSE
 ;



/******************* Interfaces / Capabilities **********************/


/* SHAP: In CORBA, an interface is a scope. While it defines
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
        INTERFACE name_def '(' type name_def ')'   // struct-level
 |      INTERFACE name_def '{' if_definitions '}'  // message-levels
 ;

if_definitions:
	if_definition
 |      if_definitions if_definition
 ;

if_definition:
        struct_dcl ';'                          // a struct
 |      except_dcl ';'                          // a struct to throw
 |      union_dcl  ';'                          // a discriminated union
 |      enum_dcl ';'                            // a set of const unsigneds
 |      TYPEDEF type name_def ';'               // names a type
 |      TYPEDEF name_def ';'                    // forward declaration
 |      const_dcl ';'                           // constant value
 |      opr_dcl ';'                             // an operation
 |      repr_dcl ';'                            // advises on representation
 ;

opr_dcl:
        ONEWAY VOID name_def '(' params ')'
 |      ret_type name_def '(' param_2s ')' opt_raises
 ;

/**
 * In expanding to the struct level, all the params are gathered into
 * a struct.  First come all the capability arguments, and then all
 * the pure data parameters.  A param cannot be of mixed data and
 * capability type.
 */
params:
        /*empty*/
 |      param_list
 ;

param_list:
        param
 |      param_list ',' param
 ;

/**
 * The parameter name is defined within the scope of this message
 * name.  In this one regard, each individual named message is also a
 * named nested scope.  This allows, for example, a following REPR
 * clause to give placement advice on a parameter by refering to it as
 * "messageName::parameterName" 
 */
param:
        type name_def
 ;

/**
 * In expanding to the oneway level, a non-VOID return type becomes a
 * first OUT argument of the same type, and the message is left with a
 * VOID return type.  The generated out parameter will be named
 * "_result", and so REPR advice can refer to the result by this name
 * even when the return type syntax is used.
 */
ret_type:
        type
 |      VOID
 ;

param_2s:
        /*empty*/
 |      param_2_list
 ;

param_2_list:
        param_2
 |      param_2_list ',' param_2
 ;

/**
 * In expanding to the oneway level, a normal (IN) parameter is left
 * alone, but the out parameters are gathered together to form the
 * normal parameter list of the oneway success message to the Resume
 * parameter. 
 */
param_2:
        param
 |      OUT param
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
opt_raises:
 /*empty*/
 |      raises
 ;

raises:
        RAISES '(' exceptions ')'
 ;

exceptions:
        /*empty*/
 |      exceptions ',' scoped_name
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
        REPR '{' advisories '}'
 |      REPR advice
 ;

advisories:
        /*empty*/
 |      advisories advice
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
        scoped_name ':' const_expr ';'
 ;


%%
