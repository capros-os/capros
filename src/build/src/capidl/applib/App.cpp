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

#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include "xmalloc.h"
#include "App.h"

struct FileList {
  char* fileName;
  int isScratch;
  FileList* next;
} ;

static struct {
  const char *appName;
  bool isAborting;
  bool isInteractive;
  unsigned long exitCode;
  FileList *fileList;
} app;

static FileList *filelist_create(const char *name); /* forward */
static void app_CleanFiles();	/* forward */

void 
app_init(const char *name)
{
  app.appName = name;
  app.fileList = 0;
  app.isAborting = false;
  app.exitCode = 0;
}

void app_destroy()
{
  app.exitCode = 1;
  app_CleanFiles();
}

void 
app_SetExitValue(unsigned long value)
{
  app.exitCode = value;
}

const char *
app_name()
{
  return app.appName;
}

void 
app_ExitWithCode(unsigned long value)
{
  app_SetExitValue(value);
  app_Exit();
}

void
app_Exit()
{
  app.isAborting = true;
  
  app_CleanFiles();
  
  exit(app.exitCode);
}

void 
app_SetInteractive()
{
  app.isInteractive = true;
}

bool app_IsInteractive()
{
  return app.isInteractive;
}

void
app_AddScratch(const char* fileName)
{
  FileList *fl = filelist_create(fileName);
  fl->next = app.fileList;
  app.fileList = fl;
  fl->isScratch = 1;
}

void
app_AddTarget(const char* fileName)
{
  FileList *fl = filelist_create(fileName);
  fl->next = app.fileList;
  app.fileList = fl;
  fl->isScratch = 0;
}

FileList *
filelist_create(const char *name)
{
  FileList *fl = MALLOC(FileList);

  fl->fileName = VMALLOC(char, strlen(name)+1);
  strcpy(fl->fileName, name);
  fl->isScratch = 0;

  return fl;
}

void
filelist_destroy(FileList *fl)
{
  free(fl->fileName);
  free(fl);
}

static void
app_CleanFiles()
{
  FileList *cur;
  while (app.fileList) {
    if (app.fileList->isScratch || app.exitCode)
      unlink(app.fileList->fileName);
    cur = app.fileList;
    app.fileList = app.fileList->next;

    filelist_destroy(cur);
  }
}

const char* app_BuildPath(const char* path)
{
  char buf[PATH_MAX];

  if (strncmp(path, "/eros/", 6) != 0) {
    strcpy(buf, path);
  }
  else {
    const char *buildroot = getenv("EROS_ROOT");
    if (buildroot) {
      strcpy(buf, buildroot);
    }
    else {
      buildroot = getenv("HOME");
      if (buildroot == 0)
	diag_fatal(-1, "Could not find $EROS_ROOT or $HOME\n");

      strcpy(buf, buildroot);
      strcat(buf, "/eros/");
    }
    strcat(buf, &path[5]);
  }

  {
    char *bufCopy = VMALLOC(char, sizeof(buf) + 1);
    strcpy(bufCopy, buf);
    return bufCopy;
  }
}
