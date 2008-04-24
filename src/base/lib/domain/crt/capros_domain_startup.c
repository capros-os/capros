/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the Capros Operating System.
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

/* Startup code for all processes. */

#include <stdlib.h>
#include <unistd.h>
#include <eros/Invoke.h>

static char *argv[] = { 0 };
static char *envp[] = { 0 };

extern int main(int, char *[], char *[]);

void 
__domain_startup(void *arg)
{
#if 0	// this pulls in malloc etc., which we want to avoid
extern void _init();
extern void _fini();
  atexit(_fini);
  _init();
  exit(main(0, argv, envp));
#else
  int status = main(0, argv, envp);
  /* Constructed processes should deconstruct themselves
   * and never return from main(). 
   * Primordial processes will return from main() when done.
   * Make them stall here. */
  _exit(status);
#endif
}
