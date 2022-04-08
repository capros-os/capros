#
# Copyright (C) 1998, 1999, Jonathan S. Shapiro.
# Copyright (C) 2006, Strawberry Development Group.
#
# This file is part of the EROS Operating System.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

# Script to create compiled-in symbol table.  This script critically
# relies on the fact that the debugging .o file is linked at the very
# end of the program and therefore does not perturb code space.  In
# particular, it trusts that for all functions of interest the function
# address does not move between 'eros' and 'eros.debug'
#
# This is wrong for the functions in 'DebuggerStubs'.


BEGIN {
  debug = 0;   # set to 0 for quiet tool
  
  digit_value["0"] = 0;
  digit_value["1"] = 1;
  digit_value["2"] = 2;
  digit_value["3"] = 3;
  digit_value["4"] = 4;
  digit_value["5"] = 5;
  digit_value["6"] = 6;
  digit_value["7"] = 7;
  digit_value["8"] = 8;
  digit_value["9"] = 9;
  digit_value["a"] = 10;
  digit_value["b"] = 11;
  digit_value["c"] = 12;
  digit_value["d"] = 13;
  digit_value["e"] = 14;
  digit_value["f"] = 15;
  digit_value["A"] = 10;
  digit_value["B"] = 11;
  digit_value["C"] = 12;
  digit_value["D"] = 13;
  digit_value["E"] = 14;
  digit_value["F"] = 15;

  FILECOUNT = 0
}

function cleanfun(s) {
  gsub(/:[fF][(),0-9=*]+$/, "", s);
  return s;
}

function basename(s) {
  while (pos = index(s, "/")) {
    s = substr(s, pos+1);
  }

  return s;
}

# Look for function name in $6 and $7, ignoring ".hidden".
function striphidden(n1, n2) {
  if (n1 == ".hidden")
    return n2;
  else
    return n1
}

# Strangely enough, gawk does not appear to grok hex numbers for
# purposes of conversion...
function hextodec(s) {
  value = 0;
  
  while (length(s) > 0) {

#    printf("s to convert is: %s\n", s);
    
    digit = substr(s, 1, 1);
    s = substr(s, 2);
    value = value * 16;
    value += digit_value[digit];
  }

#  printf("Final value is 0x%x\n", value);

  return value;
}

# array indices greater than 0x7fffffff do not work - perhaps
# it considers them negative?
# Get a value that works reliably as an array index.
function toindex(value) {
  return value % 2147483648;
}

# Following is to pick functions out of the --syms output:
$2=="l" && $3=="F" && $4==".text" {
  truepc = hextodec($1);
  pc = toindex(truepc);
  fun_name = striphidden($6, $7);
  
  if (debug)
    printf("0x%08x FUN  %s\n", truepc, fun_name);

  if (funtab[pc] && funtab[pc] != fun_name) {
      printf("[SYMS] Function table mismatch at 0x%08x old=%s new=%s\n",
	     truepc, funtab[pc], fun_name);
      exit(1);
  }

  funtab[pc] = fun_name;
  funLine[pc] = NR;
  funpc[pc] = truepc;
  funclass[pc] = "l";
}

$2=="g" && $3=="F" && $4==".text" {
  truepc = hextodec($1);
  pc = toindex(truepc);
  fun_name = striphidden($6, $7);
  
  if (debug)
    printf("0x%08x FUN  %s\n", truepc, fun_name);

  # Perversely, a weak symbol name should be accepted over a global
  # symbol name, because the weak symbol name is the one that actually
  # resolved.
  if (funclass[pc] && funclass[pc] == "w") {
    next;
  }
   
# Mismatch does happen here; ignore it.
#  if (funtab[pc] && funtab[pc] != fun_name) {
#      printf("[SYMS] Function table mismatch at 0x%08x old=%s new=%s\n",
#	     truepc, funtab[pc], fun_name);
#      exit(1);
#  }

  funtab[pc] = fun_name;
  funLine[pc] = NR;
  funpc[pc] = truepc;
  funclass[pc] = "g";
}

$2=="w" && $3=="F" && $4==".text" {
  truepc = hextodec($1);
  pc = toindex(truepc);
  fun_name = striphidden($6, $7);
  
  if (debug)
    printf("0x%08x FUN  %s\n", truepc, fun_name);

  # Perversely, a weak symbol name should be accepted over a global
  # symbol name, because the weak symbol name is the one that actually
  # resolved.

  # Mismatch does happen here; ignore it.
  # if (funtab[pc] && funtab[pc] != "g" && funtab[pc] != fun_name) {
  #     printf("[SYMSw] Function table mismatch at 0x%08x old=%s new=%s oldline=%d newline=%d\n",
	#      truepc, funtab[pc], fun_name, funLine[pc], NR);
  #     exit(1);
  # }

  funtab[pc] = fun_name;
  funLine[pc] = NR;
  funpc[pc] = truepc;
  funclass[pc] = "w";
}

$2=="SO" && NF==7 {
  truepc = hextodec($1);
  pc = toindex(truepc);

  bn = basename($7);

  if (debug)
    printf("0x%08x FILE %s\n", truepc, bn);

  if (bn in file_list) {
    file_sym_ndx = file_list[bn];
  }
  else {
    file_list[bn] = FILECOUNT;
    file_sym_ndx = FILECOUNT;
    FILECOUNT++;
  }
    
  curfile = file_sym_ndx;
  compunit = bn;
}

$2=="SO" && NF==6 {
  # End of compilation unit.  Add an entry to the table to bound line
  # numbers and the like.

  truepc = hextodec($1);
  pc = toindex(truepc);

  if (debug)
    printf("0x%08x EOCU <%s>\n", truepc, compunit);
}

$2=="SOL"   {
  truepc = hextodec($1);
  pc = toindex(truepc);
  bn = basename($7);
  
  if (debug)
    printf("0x%08x INC  %s\n", truepc, bn);

  if (bn in file_list) {
    file_sym_ndx = file_list[bn];
  }
  else {
    file_list[bn] = FILECOUNT;
    file_sym_ndx = FILECOUNT;
    FILECOUNT++;
  }
    
  curfile = file_sym_ndx;
}

$2=="FUN"   {
  truepc = hextodec($1);
  pc = toindex(truepc);

  fun_name = cleanfun($7)
    
  # the following check is correct for later compilers, but earlier
  # compilers used in RH 4.2 generated bogus function entries, which
  # would in turn cause invalid line numbers.
  #if (funtab[pc] != fun_name) {
  #  printf("[FUN] Uncaught function 0x%08x old=%s new=%s\n",
  #         truepc, funtab[pc], fun_name);
  #  exit(1);
  #}

#  fun_name = cleanfun($7);
#  
#  if (debug)
#    printf("0x%08x FUN  %s\n", truepc, fun_name);
#
#  if (funtab[$6] && funtab[$6] != fun_name) {
#    printf("String table mismatch at %d old=%s new=%s\n",
#	   $6, funtab[$6], fun_name);
#    exit(1);
#  }
#
#  funtab[$6] = fun_name;
#  funpc[$6] = truepc;

  if (funtab[pc] == fun_name) {
    cur_funpc = truepc
  }

#  if (funtab[pc])
#    printf("%s => %s\n", funtab[pc], fun_name) >> "/dev/tty";
}

$2=="SLINE" || $2=="DSLINE" || $2=="BSLINE" {
  delta = hextodec($5);
  truepc = delta + cur_funpc;
  pc = toindex(truepc);
  
  line_no[pc] = $4;
  line_file[pc] = curfile;
    
  if (debug)
    printf("0x%08x LINE %s:%d\n", truepc, basename(file_list[curfile]), $4);
}


# Once we have processed all of this shit, we need to go through it
# and output the relevant symbol tables.  One useful saving grace
# is that objdump gives us this stuff in sorted order, else we would
# need to sort here.
END {
  printf("\t.file \"symtab.S\"\n");
  printf("\t.version \"01.01\"\n");

  #
  # First, output the function table entries.  Must be in .data
  # because kernel re-sorts it on the fly.
  #
  
  printf("\n\t.section .data\n");
  printf("\t.align 4\n");
  printf("\t.globl funcSym_table\n");
  printf("funcSym_table:\n");
  
  numsymbols = 0;
  
  for (i in funtab) {
    printf("\t.long 0\n");
    printf("\t.long 0x%08x\n", funpc[i]);
    printf("\t.long .LM%d\n", i);
    printf("\t.long .LH%d\n", i);
    numsymbols++;
  }

  printf("\n\t.section .rodata\n");
  printf("\n\t.align 4\n");
  printf(".globl funcSym_count\n");
  printf("funcSym_count:\n");
  printf("\t.long %d\n", numsymbols);

  #
  # Dump the line table in such a way that the linker will
  # complete the relocations for us:
  #
  printf("\n\n\t.section .data\n");
  printf("\t.align 4\n");
  printf("\t.globl lineSym_table\n");
  printf("lineSym_table:\n");

  numlines = 0;
  for (i in line_no) {
    # the line address is delta relative to function:
    printf("\t.long 0x%08x\n", i);
    printf("\t.long %d\n", line_no[i]);
    printf("\t.long .LF%d\n", line_file[i]);
    numlines++;
  }
    
  printf("\n\t.section .rodata\n");
  printf("\n\t.align 4\n");
  printf(".globl lineSym_count\n");
  printf("lineSym_count:\n");
  printf("\t.long %d\n", numlines);

  
  #
  # Dump the file name table:
  #

  printf("\n\t.section .rodata\n");
  for (i in file_list) {
    printf(".LF%d:\n", file_list[i]);
    printf("\t.asciz \"%s\"\n", i);
  }

  #
  # Dump the *mangled* name table:
  #

  printf("\n\t.section .rodata\n");
  for (i in funtab) {
    printf(".LM%d:\n", i);
    printf("\t.asciz \"%s\"\n", funtab[i]);
  }

#  printf("\n\t.section .rodata\n");
#  printf("\n\t.align 4\n");
#  printf(".globl _7SymName.NumSymbols\n");
#  printf("_7SymName.NumSymbols:\n");
#  printf("\t.long %d\n", numsymbols);

  #
  # Finally, output the string table.  This must be last because it
  # proceeds via a pipe, and we have no way to forcibly flush that
  # pipe in order to append things after it.
  #
  # Note that not all mangled entries have a corresponding demangled
  # entry -- only those for functions.
  
  strfilt = "c++filt";
  
  for (i in funtab) {
    printf(".LH%d:\n", i) | strfilt;
    printf("\t.asciz \"%s\"\n", funtab[i]) | strfilt;
  }

  close(strfilt);

  
}
