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

#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>		/* for rename(2) */
#include <limits.h>		/* for PATH_MAX */
#include <sys/stat.h>
#include "xmalloc.h"
#include "Diag.h"
#include "path.h"

bool
path_exists(const char *s)
{
  struct stat sb;
  int result = stat(s, &sb);

  if (result == -1 && errno == ENOENT)
    return false;
  else if (result == -1) {
    diag_fatal(1, "Could not stat path\n");
    return false;
  }
  else
    return true;
}

bool
path_isdir(const char *s)
{
  struct stat sb;
  int result = stat(s, &sb);

  if (result >= 0 && S_ISDIR(sb.st_mode))
    return true;
  
  return false;
}

bool
path_isfile(const char *s)
{
  struct stat sb;
  int result = stat(s, &sb);

  if (result >= 0 && S_ISREG(sb.st_mode))
    return true;
  
  return false;
}

bool
path_islink(const char *s)
{
  struct stat sb;
  int result = stat(s, &sb);

  if (result >= 0 && S_ISLNK(sb.st_mode))
    return true;
  
  return false;
}

bool
path_isexecutable(const char *s)
{
  struct stat sb;
  int result = stat(s, &sb);

  if (result >= 0 && S_ISREG(sb.st_mode)) {
    if (access(s, X_OK) == 0)
      return true;
  }
  
  return false;
}

char *
path_join(const char *dir, const char *tail)
{
  char *s = VMALLOC(char, strlen(dir) + strlen(tail) + 2);
  strcpy(s, dir);
  strcat(s, "/");
  strcat(s, tail);

  return s;
}

char *
path_canonical(const char *path)
{
  unsigned len = 0;
  const char *p = path;
  char *s;
  char *sptr;

  while (*p) {
    while (*p == '/' && p[1] == '/')
      p++;

    p++;
    len++;
  }

  len++;			/* trailing null */

  s = VMALLOC(char, len);
  sptr = s;
  p = path;

  do {
    *sptr++ = *p;

    while (*p == '/' && p[1] == '/')
      p++;

    p++;
  } while (*p);

  *sptr = 0;

  /* Trim trailing slashes: */
  sptr--;
  while (*sptr == '/') {
    *sptr = 0;
    sptr--;
  }

  return s;
}

int
path_mkdir(const char *path)
{
  return mkdir(path, 0700);
}

int
path_smkdir(const char *path)
{
  int result;
    
  char *tmppath = strdup(path);
  char *slash = tmppath;

  // Skip leading slashes...
  while (*slash == '/')
    slash++;
  
  while ((slash = strchr(slash, '/'))) {
    *slash = 0;			// truncate the path

    result = path_mkdir(tmppath);

    if (result < 0 && errno != EEXIST)
      return result;

    *slash = '/';		// untruncate the path
    slash++;
  }

  result = mkdir(tmppath, 0777);
  if (result < 0 && errno == EEXIST)
    return 0;
  return result;
}

bool
path_should_skip_dirent(const char *s)
{
  if (s == NULL)
    diag_fatal(1, "NULL path to path_should_skip_dirent()");

  if (strcmp(s, ".") == 0)
    return true;

  if (strcmp(s, "..") == 0)
    return true;

  return false;
}

const char *
path_scratchdir()
{
#ifdef __unix__
  // FIX: This should probably check TMPDIR environment variable first.
  return "/usr/tmp";
#else
#  error "path_tmpname() not implemented"
#endif
}

size_t
path_file_length(const char *name)
{
#ifdef __unix__
  struct stat statbuf;
  if (stat(name, &statbuf) < 0) {
    diag_fatal(1, "Cannot obtain length of \"%s\"\n", name);
    return 0;
  }
  return statbuf.st_size;
#else
#  error "path_file_length() not implemented"
#endif
}
  
int
path_remove(const char *name)
{
#ifdef __unix__
  return unlink(name);
#else
#  error "path_remove() not implemented"
#endif
}

int
path_rename(const char *path1, const char *path2)
{
#ifdef __unix__
  return rename(path1, path2);
#else
#  error "path_rename() not implemented"
#endif
}

const char *
path_current_directory()
{
#ifdef __unix__
  int len = PATH_MAX;
  char * dir = VMALLOC(char,len);
  char *cwd;

  do {
    cwd =  getcwd(dir, len);
    if (cwd == NULL) {
      if (errno == ERANGE) {
	len *= 2;
	free(dir);
	dir = VMALLOC(char, len);
      }
      else
	return NULL;
    }
  } while (cwd == NULL);

  return dir;
#else
#  error "path_current_directory() not implemented"
#endif
}

const char *
path_curdir()
{
#ifdef __unix__
  return ".";
#else
#  error "path_curdir() not implemented"
#endif
}

const char *
path_parent_dir()
{
#ifdef __unix__
  return "..";
#else
#  error "path_parent_dir() not implemented"
#endif
}

bool
path_is_dir_sep(char c)
{
#ifdef __unix__
  return c == '/';
#else
#  error "path_is_dir_sep() not implemented"
#endif
}

bool
path_same_dir(const char *d1, const char *d2)
{
#ifdef __unix__
  struct stat cur_dir;
  struct stat parent_dir;

  if (stat(d1, &cur_dir) < 0)
    return false;
  if (stat(d2, &parent_dir) < 0)
    return false;

  if (cur_dir.st_ino == parent_dir.st_ino &&
      cur_dir.st_dev == parent_dir.st_dev)
    return true;

  return false;
#else
#  error "path_same_dir() not implemented"
#endif
}
