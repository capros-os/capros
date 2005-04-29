#ifndef __PATH_H__
#define __PATH_H__
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

#ifdef __cplusplus
extern "C" {
#endif

const char *path_curdir();
bool path_is_dir_sep(char);
const char *path_parent_dir();
  
bool path_should_skip_dirent(const char *s);

int path_remove(const char *path);
int path_rename(const char *path1, const char *path2);
bool path_exists(const char *path);
bool path_isdir(const char *path);
bool path_isfile(const char *path);
bool path_isexecutable(const char *path);
bool path_islink(const char *path);
int path_mkdir(const char *path);
  // "super" mkdir -- creates path recursively.
int path_smkdir(const char *path);
size_t path_file_length(const char *path);

const char *path_current_directory();
bool path_same_dir(const char *path1, const char *path2);
  
  // The implementation of this is OS specific:
const char *path_scratchdir();
#if 0
char *path_mktmpnm(const char *dir, const char *base);
#endif

  // The following return malloc'd storage:
char *path_join(const char *dir, const char *tail);	
char *path_canonical(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* __PATH_H__ */
