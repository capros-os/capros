/*
 * Copyright (C) 2002, The EROS Group, LLC.
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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "Diag.h"
#include "App.h"

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
diag_debug(int lvl, const char* msg, ...)
{
  va_list ap;

  if (lvl && lvl > diag_debuglvl)
    return;
  
  va_start(ap, msg);
    
  fflush(stdout);

  if (lvl)
    fprintf(stderr, "%s: %d ", app_name(), lvl);

  vfprintf(stderr, msg, ap);

  va_end(ap);
}

void
diag_fatal(int code, const char* msg, ...)
{
  va_list ap;

  va_start(ap, msg);
    
  fflush(stdout);

  fprintf(stderr, "%s: Fatal: ", app_name());
  vfprintf(stderr, msg, ap);

  app_SetExitValue(code);
  app_Exit();

  va_end(ap);
}

void
diag_error(int code, const char* msg, ...)
{
  va_list ap;

  va_start(ap, msg);
    
  fflush(stdout);

  fprintf(stderr, "%s: Error: ", app_name());
  vfprintf(stderr, msg, ap);

  app_SetExitValue(code);
  
  if (app_IsInteractive() == false)
    app_Exit();
  
  va_end(ap);
}

void
diag_warning(const char* msg, ...)
{
  va_list ap;

  va_start(ap, msg);
    
  fflush(stdout);
  fprintf(stderr, "%s: Warning: ", app_name());
  vfprintf(stderr, msg, ap);

  va_end(ap);
}

