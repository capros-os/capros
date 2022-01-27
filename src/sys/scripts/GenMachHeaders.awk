#
# Copyright (C) 2001, Jonathan S. Shapiro.
# Copyright (C) 2005, 2006, Strawberry Development Group.
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

# program to generate headers in machine/ that include the appropriate
# architecture-specific header.

BEGIN {
  for (i = 1; i < ARGC; i++)
    fn[i-1]=ARGV[i];

  nFile = ARGC-1;
  ARGC=1;
  nArch = 0;
}

/^[ \t]*#.*/   { next; }

/^[ \t]*$/   { next; }

NF != 0 {
  arch[nArch] = $1;
  nArch = nArch + 1;
}

END {
  for (i = 0; i < nFile; i++) {
    print "Processing " fn[i];
    print "/* This file is generated. Do not edit. */\n" > fn[i];
    printf("#ifndef __EROS_MACHINE_%s__\n", nmgen(fn[i])) > fn[i];
    printf("#define __EROS_MACHINE_%s__\n", nmgen(fn[i])) > fn[i];

    for (a = 0; a < nArch; a++) {
      print "Processing " arch[a];

      if (a == 0) {
        printf("\n#if") > fn[i];
      }
      else {
        printf("#elif") > fn[i];
      }
      printf(" defined(EROS_TARGET_%s)\n", arch[a]) > fn[i];

      printf("#include \"../arch/%s/%s\"\n", arch[a], fn[i]) > fn[i];
    }
    printf("#else\n") > fn[i];
    printf("#error \"Unknown target\"\n") > fn[i];
    printf("#endif\n\n") > fn[i];

    printf("#endif /* __EROS_MACHINE_%s__ */\n", nmgen(fn[i])) > fn[i];
  }
}

function nmgen(nm)
{
  nm = toupper(nm);
  gsub(/[.-]/, "_", nm);

  return nm;
}
