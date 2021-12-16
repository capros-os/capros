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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <erosimg/Diag.h>
#include <erosimg/App.h>

int diag_debuglvl = 0;

void
diag_printf(const char* msg, ...)
{
  va_list ap;

  va_start(ap, msg);
    
  vfprintf(stdout, msg, ap);

  va_end(ap);
}

void
diag_printOid(OID oid)
{
  diag_printf("0x%08x%08x", (uint32_t) (oid >> 32), (uint32_t) oid);
}

void
diag_printCount(ObCount count)
{
  diag_printf("0x%08x", count);
}

void
diag_debug(int lvl, const char* msg, ...)
{
  va_list ap;

  if (lvl && lvl > diag_debuglvl)
    return;
  
  va_start(ap, msg);
    
  if (lvl)
    fprintf(stderr, "%s: L%-3d ", appName, lvl);

  vfprintf(stderr, msg, ap);

  va_end(ap);
}

void
diag_fatal(int code, const char* msg, ...)
{
  va_list ap;

  va_start(ap, msg);
    
  fprintf(stderr, "%s: Fatal: ", appName);
  vfprintf(stderr, msg, ap);

  app_ExitWithCode(code);

  va_end(ap);
}

void
diag_error(int code, const char* msg, ...)
{
  va_list ap;

  va_start(ap, msg);
    
  fprintf(stderr, "%s: Error: ", appName);
  vfprintf(stderr, msg, ap);

  app_SetExitCode(code);
  
  if (!app_IsInteractive())
    app_Exit();
  
  va_end(ap);
}

void
diag_warning(const char* msg, ...)
{
  va_list ap;

  va_start(ap, msg);
    
  fprintf(stderr, "%s: Warning: ", appName);
  vfprintf(stderr, msg, ap);

  va_end(ap);
}

