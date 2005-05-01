/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DB_NO_NMHACK

#include <arch-kerninc/db_machdep.h>		/* data types */

#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <kerninc/util.h>
#include <kerninc/SymNames.h>

/*
 * An a.out symbol table as loaded into the kernel debugger:
 *
 * symtab	-> size of symbol entries, in bytes
 * sp		-> first symbol entry
 *		   ...
 * ep		-> last symbol entry + 1
 * strtab	== start of string table
 *		   size of string table in bytes,
 *		   including this word
 *		-> strings
 */

#ifdef	SYMTAB_SPACE
int db_symtabsize = SYMTAB_SPACE;
int db_symtab[SYMTAB_SPACE/sizeof(int)] = { 0, 1 };
#endif

/*
 * Find the symbol table and strings; tell ddb about them.
 */
void
X_db_sym_init(char * symtab,	/* pointer to start of symbol table */
	      char * esymtab,	/* pointer to end of string table,
				   for checking - rounded up to integer
				   boundary */
	      const char * name
	      )
{
  db_add_symbol_table(symtab, esymtab, name, 0 /* ref */);
}

db_sym_t
X_db_sym_match(db_symtab_t * stb/*stab */, const char * symstr)
{
  uint32_t i = 0;

  for (i = 0; i < funcSym_count; i++) {
    const char *rawname = funcSym_table[i].raw_name;

    while (*rawname) {
      if (*rawname == *symstr && (strncmp(rawname, symstr, strlen(symstr)) == 0))
	db_printf("%s: %s\n", funcSym_table[i].name,
		  funcSym_table[i].raw_name);

      rawname++;
    }
  }
  
  return 0;
}

db_sym_t
X_db_lookup(db_symtab_t * srb/*stab */, const char * symstr)
{
  uint32_t i = 0;

  for (i = 0; i < funcSym_count; i++) {
    if (strcmp(funcSym_table[i].raw_name, symstr) == 0)
      return (db_sym_t) &funcSym_table[i];
    if (strcmp(funcSym_table[i].name, symstr) == 0)
      return (db_sym_t) &funcSym_table[i];
  }
  
  return 0;
}

extern int _start;
extern int etext;
extern int end;



db_sym_t
X_db_search_symbol(db_symtab_t * smtb/* symtab */,
		   register db_addr_t off,
		   db_strategy_t stg/* strategy */,
		   vm_offset_t * diffp) /* in/out */
{
  uint32_t address = off;
  
  uint32_t offset = ~0u;		/* maxword */

  struct FuncSym *pEntry = 0;

  uint32_t i = 0;
  uint32_t newOffset = 0;
  
  if ( (address < (uint32_t) &_start) || (address >= (uint32_t) &etext) )
    return 0;

  for (i = 0; i < funcSym_count; i++) {
    if (funcSym_table[i].address > address)
      continue;
    
    newOffset = address - funcSym_table[i].address;

    if (newOffset < offset) {
      pEntry = &funcSym_table[i];
      offset = newOffset;
    }
  }

  
  *diffp = offset;
  return (db_sym_t) pEntry;
}

/*
 * Return the name and value for a symbol.
 */
void
X_db_symbol_values(db_sym_t sym, const char ** namep,
		   db_expr_t *valuep)
{
  struct FuncSym *sn = (struct FuncSym *) sym;
  
  if (sn == 0) {
    *namep = "";
    *valuep = 0;
  }
  
  /* printf("sym value \"%s\" @ 0x%08x\n", sn->name, sn->address); */
    
  *namep = sn->name;
  *valuep = sn->address;
}

bool
X_db_line_at_pc(db_symtab_t * smtb/* symtab */, db_sym_t  csym/* cursym */,
		const char ** filename, int * linenum,
		db_expr_t off)
{
  uint32_t address = off;
  uint32_t offset = ~0u;		/* maxword */
  uint32_t i = 0;
  uint32_t newOffset = 0;

  struct LineSym *pEntry = 0;
  
  if ( (address < (uint32_t) &_start) || (address >= (uint32_t) &etext) )
    return false;

  for (i = 0; i < lineSym_count; i++) {
    if (lineSym_table[i].address > address)
      continue;
    
    newOffset = address - lineSym_table[i].address;

    if (newOffset < offset) {
      pEntry = &lineSym_table[i];
      offset = newOffset;
    }
  }

  if (pEntry) {
    *filename = pEntry->file;
    *linenum = pEntry->line;

    return true;
  }
  else
    return false;
}

bool
X_db_sym_numargs(db_symtab_t * stb/* symtab */, db_sym_t csm/* cursym */,
		 int * nargp, const char ** agmp/* argnamep */)
{
  *nargp = 0;
  return false;
}

/*
 * Initialization routine for a.out files.
 */
void
ddb_init()
{
  X_db_sym_init(0, 0, "nm_table");
}

#endif	/* DB_NO_NMHACK */
