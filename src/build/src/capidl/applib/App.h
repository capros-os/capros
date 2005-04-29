#ifndef __APP_H__
#define __APP_H__

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

#include "Intern.h"
#include "Diag.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_init(const char *name);

const char* app_name();
  
void app_AddScratch(const char* name);
void app_AddTarget(const char* name);

bool app_Aborting();
bool app_IsInteractive();
void app_SetExitValue(unsigned long exitValue);
void app_Exit() __attribute__ ((noreturn));

void app_ExitWithCode(unsigned long value);
void app_SetInteractive();

const char *app_BuildPath(const char*);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */
