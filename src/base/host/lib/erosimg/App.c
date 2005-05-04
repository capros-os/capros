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

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <erosimg/App.h>

static bool isInteractive = false;
static uint32_t exitCode = 0;
bool isAborting = false;
const char *appName;

typedef struct FileList FileList;
struct FileList {
  char* fileName;
  int isScratch;
  FileList* next;
} ;

FileList *mk_fileList(const char *nm)
{
  FileList *fl = (FileList *) malloc(sizeof(FileList *));
  fl->fileName = strdup(nm);
  fl->isScratch = 0;

  return fl;
}

static FileList *fileList = 0;

static void CleanFiles()
{
  while (fileList) {
    FileList *cur;

    if (fileList->isScratch || exitCode)
      unlink(fileList->fileName);

    cur = fileList;
    fileList = fileList->next;

    free(cur->fileName);
    free(cur);
  }
}

void
app_AddScratch(const char* fileName)
{
  FileList *fl = mk_fileList(fileName);
  fl->next = fileList;
  fileList = fl;
  fl->isScratch = 1;
}

void
app_AddTarget(const char* fileName)
{
  FileList *fl = mk_fileList(fileName);
  fl->next = fileList;
  fileList = fl;
  fl->isScratch = 0;
}


void 
app_SetExitCode(uint32_t code)
{
  exitCode = code;
}

void 
app_SetInteractive(bool b)
{
  isInteractive = b;
}

bool
app_IsInteractive()
{
  return isInteractive;
}

bool
app_IsAborting()
{
  return isAborting;
}

void
app_Init(const char *nm)
{
  appName = nm;
}

const char* 
app_BuildPath(const char* path)
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

  return strdup(buf);
}

void
app_Exit()
{
  isAborting = true;
  
  CleanFiles();
  
  exit(exitCode);
}
