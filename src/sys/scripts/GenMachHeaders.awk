#
# Copyright (C) 2001, Jonathan S. Shapiro.
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

# program to generate ArchDescrip stuff for EROSIMG library.
# Note that this version handles only one architecture, which
# is something we really should fix someday.

BEGIN {
  for (i = 1; i < ARGC; i++)
    fn[i-1]=ARGV[i];

  nFile = ARGC-1;
  ARGC=1;
  nArch = 0;
}

/^[ \t]*\#.*/   { next; }

/^[ \t]*$/   { next; }

NF == 2 {
  arch[nArch] = $1;
  test[nArch] = $2;
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
        printf("\n#if defined(%s)\n", test[a]) > fn[i];
      }
      else {
        printf("#elif defined(%s)\n", test[a]) > fn[i];
      }

      printf("#include \"../%s/%s\"\n", arch[a], fn[i]) > fn[i];
    }
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
