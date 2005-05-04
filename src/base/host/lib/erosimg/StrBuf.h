#ifndef __STRBUF_H__
#define __STRBUF_H__

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

#include <eros/target.h>

typedef struct StrBuf StrBuf;

#ifdef __cplusplus
extern "C" {
#endif

StrBuf *strbuf_create();
void strbuf_append(StrBuf *, const char *);
void strbuf_append_int(StrBuf *, int i);
void strbuf_append_unsigned(StrBuf *, unsigned u);
void strbuf_append_char(StrBuf *, char c);
void strbuf_destroy(StrBuf *);
const char *strbuf_asString(StrBuf *);

#ifdef __cplusplus
}
#endif

#endif /* __STRBUF_H__ */
