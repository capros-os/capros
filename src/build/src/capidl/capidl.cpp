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

#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include <gmp.h>  // GNU multiple precision library
#include <malloc.h>
#include <dirent.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <applib/xmalloc.h>
#include <applib/App.h>
#include <applib/Intern.h>
#include <applib/path.h>
#include <vector>

#include "SymTab.h"
#include "ParseType.h"
#include "Lexer.h"
#include "idl.h"
#include "backend.h"

bool showparse = false;
bool opt_dispatchers = false;
extern int yyparse(void *);

// Include locations specified on the command line with -I:
static std::vector<const char *> searchPath;

static std::vector<TopsymMap*> uocMap;


TopsymMap *
topsym_create(InternedString s, InternedString f, bool isCmdLine)
{
  TopsymMap * ts = new TopsymMap(s, f, isCmdLine);

  uocMap.push_back(ts);

  return ts;
}


const char *
basename(const char *s)
{
  const char *slash;
  slash = strrchr(s, '/');

  if (slash)
    return slash + 1;
  else
    return s;
}

void
pkgwalker(Symbol *scope, BackEndFn outfn)
{
  /* Export subordinate packages first! */
  for (const auto eachChild : scope->children) {
    if (eachChild->cls != sc_package)
      outfn(eachChild);

    if (eachChild->cls == sc_package)
      pkgwalker(eachChild, outfn);
  }
}

void
parse_file(InternedString fileName, bool isCmdLine)
{
  FILE *fin = fopen(fileName, "r");
  Symbol *usingScope;
  MyLexer *myLexer;

  if (fin == NULL)
    diag_fatal(1, "Couldn't open description file \"%s\"\n",
	       fileName);

  usingScope = symbol_curScope;
  symbol_curScope = 0;

  myLexer = mylexer_create(fin, isCmdLine);
  if (showparse)
    mylexer_setDebug(showparse);

  myLexer->current_basename = basename(fileName);
  myLexer->current_file = app_BuildPath(fileName);

  yyparse(myLexer);
  
  fclose(fin);

  if (myLexer->num_errors != 0u) {
    app_SetExitValue(1u);

    app_Exit();
  }

  symbol_curScope = usingScope;
}

unsigned
contains(InternedString scope, InternedString sym)
{
  char nextc;

  unsigned scopeLen = strlen(scope);

  if (scopeLen > strlen(sym))
    return 0;

  if (strncmp(scope, sym, scopeLen) != 0)
    return 0;

  nextc = sym[scopeLen];

  if (nextc == '.' || nextc == 0)
    return scopeLen;
  return 0;
}

void
import_uoc(InternedString ident)
{
  Symbol *sym = symbol_LookupChild(symbol_UniversalScope, ident, 0);

  if (sym) {
    /* If we find the symbol and it is marked done, we have already
       loaded this unit of compilation and we can just return. */
    if (sym->complete)
      return;
    
    diag_fatal(1, "Recursive dependency on \"%s\"\n",
		ident);

  }

  for (const auto eachTS : uocMap) {
    InternedString scopeName = eachTS->symName;
    
    if (contains(scopeName,ident)) {
      parse_file(eachTS->fileName, eachTS->isCmdLine);
      return;
    }
  }
}

InternedString
lookup_containing_file(Symbol *s)
{
  InternedString uocName;

  s = symbol_UnitOfCompilation(s);
  uocName = symbol_QualifiedName(s, '.');

  for (const auto eachTS : uocMap) {
    if(eachTS->symName == uocName)
      return eachTS->fileName;
  }

  return 0;
}

/** Locate the containing UOC for a symbol and import it.

    Our handling of packages is slightly different from that of Java,
    with the consequence that there isn't any ambiguity about
    containership nesting. Every file exports a single top-level name,
    and we are checking here against the top-level names rather than
    against the package names.

    As a FUTURE optimization, we will use the isUOC field in the
    TopsymMap to perform lazy file prescanning.
*/

void
import_symbol(InternedString ident)
{
  /* First, see if the symbol is defined in one of the input files. 
     This is true exactly if the symbol provided by the input file is
     a identifier-wise substring of the desired symbol. */ 

  for (const auto eachTS : uocMap) {
    InternedString scopeName = eachTS->symName;
    
    if (contains(scopeName,ident))
      import_uoc(scopeName);
  }
}

void
prescan(const char *fileName, bool isCmdLine)
{
  const char *path = app_BuildPath(fileName);
  FILE *fin = fopen(path, "r");
  PrescanLexer *lexer;

  if (fin == NULL)
    diag_fatal(1, "Couldn't open description file \"%s\"\n",
	       fileName);

  lexer = prescanlexer_create(fileName, isCmdLine, fin);
  if (showparse)
    prescanlexer_setDebug(showparse);

  {
    ParseType pt;

    prescan_lex(&pt, lexer);
  }

  fclose(fin);
}

void
prescan_includes(const char *dirPath)
{
  DIR *dir = opendir(dirPath);
  struct dirent *de;

  if (dir == NULL)
    diag_fatal(1, "Directory %s: %s\n", dirPath, strerror(errno));

  while ((de = readdir(dir))) {
    const char *ent = de->d_name;
    char *entpath;
    
    if (path_should_skip_dirent(ent))
      continue;

    entpath = path_join(dirPath, ent);
    
    {
      const char *dotIdl = strstr(entpath, ".idl");

      if (path_isdir(entpath))
	prescan_includes(entpath);
      else if (dotIdl && (strcmp(dotIdl, ".idl") == 0))
	prescan(entpath, false);
    }

    free(entpath);
  }

  closedir(dir);
}

const char *outputFileName = 0;
const char *target = ".";
const char *execDir = 0;
const char *archiveName = 0;
const char *cHeaderName = "changeme.h";

int
main(int argc, char *argv[])
{
  int c;
  extern int optind;
#if 0
  extern char *optarg;
#endif
  backend *be;
  int opterr = 0;
  const char *lang = 0;
  bool verbose = false;
  
  app_init("capidl");

  while ((c = getopt(argc, argv, 
		     "D:X:A:sdvl:o:I:h:"
		     )) != -1) {

    switch(c) {
    case 'v':
      verbose = true;
      break;

    case 'D':
      target = path_canonical(optarg);
      break;

    case 'X':
      execDir = path_canonical(optarg);
      break;

    case 'A':
      archiveName = path_canonical(optarg);
      break;

    case 'l':
      lang = optarg;
      break;

    case 'o':
      outputFileName = optarg;
      break;

    case 'I':
      searchPath.push_back(optarg);
      break;

    case 'd':
      showparse = true;
      break;

    case 's':
      opt_dispatchers = true;
      break;

    case 'h':
      cHeaderName = optarg;
      break;

    default:
      opterr++;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argc == 0)
    opterr++;

#if 0
  if (target == 0)
    opterr++;
#endif

  if (opterr)
    diag_fatal(1, "Usage: capidl -D target-dir [-v] [-d] [-nostdinc] [-Idir] [-Ddef] "
		"[-Aassert] [-o server-file.c] [-o server-header-name.h] -l language idl_file\n");

  if (verbose)
    app_SetInteractive();

  be = resolve_backend(lang);

  if (be == 0) 
    diag_fatal(1, "Output language is not recognized.\n");

  /* Must prescan the command line units of compilation first, so that
     they get marked as units of compilation. */
  for (int i = 0; i < argc; i++)
    prescan(argv[i], true);

  for (const auto eachPath : searchPath)
    prescan_includes(eachPath);

  symbol_InitSymtab();

  for (const auto eachTS : uocMap) {
    InternedString ident = eachTS->symName;

    import_uoc(ident);
  }

  symbol_QualifyNames(symbol_UniversalScope);

  if (!symbol_ResolveReferences(symbol_UniversalScope))
    diag_fatal(1, "Symbol reference resolution could not be completed.\n");

  symbol_ResolveIfDepth(symbol_UniversalScope);

  if (!symbol_TypeCheck(symbol_UniversalScope))
    diag_fatal(1, "Type errors are present.\n");

  if (be->typecheck && !be->typecheck(symbol_UniversalScope))
    diag_fatal(1, "Target-specific type errors are present.\n");

  if (!symbol_IsLinearizable(symbol_UniversalScope))
    diag_fatal(1, "Circular dependencies are present.\n");

  if (be->xform) be->xform(symbol_UniversalScope);

  if (be->scopefn)
    be->scopefn(symbol_UniversalScope, be->fn);
  else
    pkgwalker(symbol_UniversalScope, be->fn);
  
  app_Exit();
}
