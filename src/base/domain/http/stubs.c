/*
 * Copyright (C) 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Stubs for procedures required by openssl. */

#include <stddef.h>
#include "http.h"
#include <errno.h>

#ifndef SELF_TEST

int raise(int signo) {
  DBGPRINT(DBGTARGET, "HTTP: raise %d\n", signo);
  errno = ESRCH;
  return -1;
}

#ifdef EROS_TARGET_arm
int _isatty(int fd) {
#else
int isatty(int fd) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: isatty %d\n", fd);
  return 0;
}
#if EROS_TARGET_arm
int _stat(char *fn, int *stat) {
#else
int stat(char *fn, int *stat) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _stat %s\n", fn);
  errno = ENOENT;
  return -1;
}

int _fstat(int fd, int *stat) {
  DBGPRINT(DBGTARGET, "HTTP: _fstat\n");
  errno = EIO;
  return -1;
}

int kill(int pid, int signo) {
  DBGPRINT(DBGTARGET, "HTTP: _kill pid= %d, sig=%d\n", pid, signo);
  errno = ESRCH;
  return -1;
}

int getpid() {
  //DBGPRINT(DBGTARGET, "HTTP: getpid\n");
  return 55;
}

#if EROS_TARGET_arm
int _lseek(int fd, int offset, int whence) {
#else
int lseek(int fd, int offset, int whence) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _lseek fd= %d, off=%d whence=%d\n",
	   fd, offset, whence);
  errno = ESPIPE;
  return -1;
}

int fstat(int fd, int *stat) {
  DBGPRINT(DBGTARGET, "HTTP: _fstat %d\n", fd);
  errno = EBADF;
  return -1;
}

#if EROS_TARGET_arm
int _gettimeofday(int tv, int tz) {
#else
int gettimeofday(int tv, int tz) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _gettimeofday\n");
  errno = EFAULT;
  return -1;
}

#if EROS_TARGET_arm
int _read(int fd, void *buf, size_t len) {
#else
int read(int fd, void *buf, size_t len) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _read fd=%d\n", fd);
  return 0;  // Signal EOF
}

#if EROS_TARGET_arm
int _write(int fd, void *buf, size_t len) {
#else
int write(int fd, void *buf, size_t len) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _read fd=%d\n", fd);
  return len;  // Say we wrote it
}

#if EROS_TARGET_arm
int _open(char *path, int flags, int mode) {
#else
int open(char *path, int flags, int mode) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _open fn=%s\n", path);
  errno = ENOENT;
  return -1;
}

#if EROS_TARGET_arm
int _close(int fd) {
#else
int close(int fd) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _close %d\n", fd);
  errno = EBADF;
  return -1;
}

int select(int fdno, void *fds, void *timeout) {
  return fdno;
}

int getuid(void) {
  DBGPRINT(DBGTARGET, "HTTP: getuid\n");
  return 55;
}

typedef void (*sig_t) (int);
sig_t signal(int sig, sig_t func) {
  DBGPRINT(DBGTARGET, "HTTP: signal=%d, func=%d\n", sig, func);
  return 0;
}

#endif
