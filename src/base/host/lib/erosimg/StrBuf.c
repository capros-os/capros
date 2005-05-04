/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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

#include <assert.h>
#include <memory.h>
#include <malloc.h>
#include <stdio.h>

#include <erosimg/StrBuf.h>

struct StrBuf {
  char *buf;
  size_t len;
  size_t bound;
};

#define STRBUF_BUFSZ 1024

static void
strbuf_grow(StrBuf *sb)
{
  sb->bound += STRBUF_BUFSZ;
  sb->buf = realloc(sb->buf, sb->bound);
  sb->buf[0] = 0;
}

StrBuf *
strbuf_create()
{
  StrBuf *sb = (StrBuf *) malloc(sizeof(StrBuf));

  sb->len = 0;
  sb->bound = 0;
  sb->buf = 0;

  return sb;
}

void
strbuf_append(StrBuf *sb, const char *s)
{
  size_t slen = strlen(s);
  size_t avail = sb->bound - sb->len;

  while (avail < (slen+1)) {
    strbuf_grow(sb);
    avail = sb->bound - sb->len;
  }

  strcpy(sb->buf + sb->len, s);
  sb->len += slen;
}

void
strbuf_append_int(StrBuf *sb, int i)
{
  char str[20];
  sprintf(str, "%d", i);

  strbuf_append(sb, str);
}

void
strbuf_append_unsigned(StrBuf *sb, unsigned u)
{
  char str[20];
  sprintf(str, "%u", u);

  strbuf_append(sb, str);
}

void
strbuf_append_char(StrBuf *sb, char c)
{
  char str[2];
  str[0] = c;
  str[1] = 0;

  strbuf_append(sb, str);
}

void
strbuf_destroy(StrBuf *sb)
{
  free(sb->buf);
  free(sb);
}

const char *
strbuf_asString(StrBuf *sb)
{
  return sb->buf;
}
