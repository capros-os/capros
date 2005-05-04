#ifndef __APP_H__
#define __APP_H__

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

#include <eros/target.h>
#include <erosimg/Diag.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char *appName;

void app_Init(const char *nm);
void app_SetExitCode(uint32_t);
void app_AddScratch(const char* name);
void app_AddTarget(const char* name);

#if 0
void app_SetAborting(bool);
#endif
bool app_IsAborting();

void app_SetInteractive(bool);
bool app_IsInteractive();
void app_Exit();

INLINE void 
app_ExitWithCode(uint32_t code)
{
  app_SetExitCode(code);
  app_Exit();
}

const char *app_BuildPath(const char *);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */
